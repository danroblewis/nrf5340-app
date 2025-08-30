#include "wasm_service.h"
#include "../wasm3_wrapper.h"
#include <zephyr/sys/printk.h>
#include <zephyr/sys/crc.h>
#include <string.h>

/**
 * @file wasm_service.c
 * @brief Custom WASM Service implementation
 */

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

/* WASM memory buffer - statically allocated for deterministic memory usage */
static uint8_t wasm_code_buffer[WASM_CODE_BUFFER_SIZE];
static uint32_t wasm_code_size = 0;
static uint32_t wasm_bytes_received = 0;
static uint32_t wasm_total_expected = 0;
static uint8_t wasm_upload_sequence = 0;

/* Service state */
static uint8_t wasm_status = WASM_STATUS_IDLE;
static uint8_t wasm_error_code = WASM_ERROR_NONE;
static struct bt_conn *wasm_conn = NULL;

/* WASM3 runtime */
static wasm3_runtime_t wasm_runtime;
static bool wasm_runtime_initialized = false;

/* Last execution result */
static wasm_result_packet_t last_result;
static bool last_result_valid = false;

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Validate WASM magic number (0x00 0x61 0x73 0x6d)
 */
static bool validate_wasm_magic(const uint8_t *data, size_t size)
{
    if (size < 4) {
        return false;
    }
    
    return (data[0] == 0x00 && data[1] == 0x61 && 
            data[2] == 0x73 && data[3] == 0x6d);
}

/**
 * @brief Initialize WASM3 runtime if not already done
 */
static int ensure_wasm_runtime(void)
{
    if (wasm_runtime_initialized) {
        return 0;
    }
    
    wasm3_config_t config = {
        .stack_size = 8192,     /* 8KB stack */
        .heap_size = 8192,      /* 8KB heap */
        .enable_tracing = 0
    };
    
    int ret = wasm3_init(&wasm_runtime, &config);
    if (ret != WASM3_SUCCESS) {
        printk("WASM Service: Failed to initialize WASM3 runtime: %d\n", ret);
        wasm_error_code = WASM_ERROR_LOAD_FAILED;
        return ret;
    }
    
    wasm_runtime_initialized = true;
    printk("WASM Service: WASM3 runtime initialized\n");
    return 0;
}

/**
 * @brief Load and compile WASM module
 */
static int load_wasm_module(void)
{
    if (wasm_code_size == 0) {
        printk("WASM Service: No WASM code to load\n");
        wasm_error_code = WASM_ERROR_INVALID_PARAMS;
        return -1;
    }
    
    /* Ensure runtime is initialized */
    int ret = ensure_wasm_runtime();
    if (ret != 0) {
        return ret;
    }
    
    /* Validate WASM magic number */
    if (!validate_wasm_magic(wasm_code_buffer, wasm_code_size)) {
        printk("WASM Service: Invalid WASM magic number\n");
        wasm_error_code = WASM_ERROR_INVALID_MAGIC;
        return -1;
    }
    
    /* Load WASM module */
    ret = wasm3_load_module(&wasm_runtime, wasm_code_buffer, wasm_code_size);
    if (ret != WASM3_SUCCESS) {
        printk("WASM Service: Failed to load WASM module: %d\n", ret);
        wasm_error_code = WASM_ERROR_LOAD_FAILED;
        return ret;
    }
    
    /* Compile module */
    ret = wasm3_compile_module(&wasm_runtime);
    if (ret != WASM3_SUCCESS) {
        printk("WASM Service: Failed to compile WASM module: %d\n", ret);
        wasm_error_code = WASM_ERROR_COMPILE_FAILED;
        return ret;
    }
    
    printk("WASM Service: WASM module loaded and compiled successfully (%u bytes)\n", 
           wasm_code_size);
    wasm_status = WASM_STATUS_LOADED;
    wasm_error_code = WASM_ERROR_NONE;
    
    return 0;
}

/**
 * @brief Reset upload state
 */
static void reset_upload_state(void)
{
    wasm_code_size = 0;
    wasm_bytes_received = 0;
    wasm_total_expected = 0;
    wasm_upload_sequence = 0;
    wasm_status = WASM_STATUS_IDLE;
    wasm_error_code = WASM_ERROR_NONE;
    last_result_valid = false;
    memset(wasm_code_buffer, 0, sizeof(wasm_code_buffer));
    memset(&last_result, 0, sizeof(last_result));
}

/* ============================================================================
 * BLE CHARACTERISTIC HANDLERS
 * ============================================================================ */

/**
 * @brief Handle WASM upload packets
 */
static ssize_t wasm_upload_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len < sizeof(wasm_upload_packet_t)) {
        printk("WASM Service: Upload packet too small (%d < %zu)\n", len, sizeof(wasm_upload_packet_t));
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    const wasm_upload_packet_t *packet = (const wasm_upload_packet_t *)buf;
    
    printk("WASM Service: Upload packet received (cmd: 0x%02x, seq: %d, size: %d)\n",
           packet->cmd, packet->sequence, packet->chunk_size);
    
    switch (packet->cmd) {
    case WASM_CMD_START_UPLOAD:
        printk("WASM Service: Starting new upload (total: %u bytes)\n", packet->total_size);
        
        if (packet->total_size > WASM_CODE_BUFFER_SIZE) {
            printk("WASM Service: Upload too large (%u > %u)\n", 
                   packet->total_size, WASM_CODE_BUFFER_SIZE);
            wasm_error_code = WASM_ERROR_BUFFER_OVERFLOW;
            wasm_status = WASM_STATUS_ERROR;
            return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
        }
        
        reset_upload_state();
        wasm_total_expected = packet->total_size;
        wasm_status = WASM_STATUS_RECEIVING;
        wasm_upload_sequence = 0;
        
        /* Fall through to process data in first packet */
        __fallthrough;
        
    case WASM_CMD_CONTINUE_UPLOAD:
        if (wasm_status != WASM_STATUS_RECEIVING) {
            printk("WASM Service: Not in receiving state\n");
            wasm_error_code = WASM_ERROR_INVALID_PARAMS;
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
        }
        
        /* Verify sequence number */
        if (packet->sequence != wasm_upload_sequence) {
            printk("WASM Service: Sequence mismatch (expected %d, got %d)\n",
                   wasm_upload_sequence, packet->sequence);
            wasm_error_code = WASM_ERROR_INVALID_PARAMS;
            wasm_status = WASM_STATUS_ERROR;
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
        }
        
        /* Check buffer overflow */
        if (wasm_bytes_received + packet->chunk_size > WASM_CODE_BUFFER_SIZE ||
            wasm_bytes_received + packet->chunk_size > wasm_total_expected) {
            printk("WASM Service: Buffer overflow during upload\n");
            wasm_error_code = WASM_ERROR_BUFFER_OVERFLOW;
            wasm_status = WASM_STATUS_ERROR;
            return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
        }
        
        /* Copy chunk data */
        memcpy(wasm_code_buffer + wasm_bytes_received, packet->data, packet->chunk_size);
        wasm_bytes_received += packet->chunk_size;
        wasm_upload_sequence++;
        
        printk("WASM Service: Received chunk %d (%u / %u bytes)\n",
               packet->sequence, wasm_bytes_received, wasm_total_expected);
        
        /* Check if upload is complete */
        if (wasm_bytes_received >= wasm_total_expected) {
            wasm_code_size = wasm_bytes_received;
            wasm_status = WASM_STATUS_RECEIVED;
            printk("WASM Service: Upload complete, loading module...\n");
            
            /* Automatically load and compile the module */
            if (load_wasm_module() == 0) {
                printk("WASM Service: WASM module ready for execution\n");
            }
        }
        break;
        
    case WASM_CMD_END_UPLOAD:
        if (wasm_status == WASM_STATUS_RECEIVING) {
            wasm_code_size = wasm_bytes_received;
            wasm_status = WASM_STATUS_RECEIVED;
            printk("WASM Service: Upload ended by client, loading module...\n");
            
            if (load_wasm_module() == 0) {
                printk("WASM Service: WASM module ready for execution\n");
            }
        }
        break;
        
    case WASM_CMD_RESET:
        printk("WASM Service: Reset requested\n");
        reset_upload_state();
        if (wasm_runtime_initialized) {
            wasm3_cleanup(&wasm_runtime);
            wasm_runtime_initialized = false;
        }
        break;
        
    default:
        printk("WASM Service: Unknown upload command: 0x%02x\n", packet->cmd);
        wasm_error_code = WASM_ERROR_INVALID_PARAMS;
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    return len;
}

/**
 * @brief Handle WASM execution requests
 */
static ssize_t wasm_execute_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len < sizeof(wasm_execute_packet_t)) {
        printk("WASM Service: Execute packet too small (%d < %zu)\n", len, sizeof(wasm_execute_packet_t));
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    const wasm_execute_packet_t *packet = (const wasm_execute_packet_t *)buf;
    
    printk("WASM Service: Execute request for function '%s' with %u args\n",
           packet->function_name, packet->arg_count);
    
    /* Clear previous result */
    memset(&last_result, 0, sizeof(last_result));
    last_result_valid = false;
    
    /* Check if WASM is ready */
    if (wasm_status != WASM_STATUS_LOADED) {
        printk("WASM Service: WASM not loaded (status: %d)\n", wasm_status);
        last_result.status = WASM_STATUS_ERROR;
        last_result.error_code = WASM_ERROR_LOAD_FAILED;
        last_result_valid = true;
        return len;
    }
    
    /* Validate function name */
    if (strnlen(packet->function_name, WASM_FUNCTION_NAME_SIZE) >= WASM_FUNCTION_NAME_SIZE) {
        printk("WASM Service: Function name too long\n");
        last_result.status = WASM_STATUS_ERROR;
        last_result.error_code = WASM_ERROR_INVALID_PARAMS;
        last_result_valid = true;
        return len;
    }
    
    /* Update status */
    wasm_status = WASM_STATUS_EXECUTING;
    
    /* Record start time */
    uint32_t start_time = k_uptime_get_32();
    
    /* Execute function */
    int32_t result_value = 0;
    int ret = wasm3_call_function(&wasm_runtime, packet->function_name, 
                                 packet->args, packet->arg_count, &result_value);
    
    /* Calculate execution time */
    uint32_t end_time = k_uptime_get_32();
    uint32_t execution_time_us = (end_time - start_time) * 1000; /* Convert to microseconds */
    
    /* Fill result packet */
    last_result.return_value = result_value;
    last_result.execution_time_us = execution_time_us;
    
    if (ret == WASM3_SUCCESS) {
        printk("WASM Service: Function executed successfully, result: %d\n", result_value);
        last_result.status = WASM_STATUS_COMPLETE;
        last_result.error_code = WASM_ERROR_NONE;
        wasm_status = WASM_STATUS_LOADED; /* Ready for next execution */
    } else {
        printk("WASM Service: Function execution failed: %d\n", ret);
        last_result.status = WASM_STATUS_ERROR;
        
        /* Map WASM3 errors to service errors */
        switch (ret) {
        case WASM3_ERROR_EXECUTION_FAILED:
            last_result.error_code = WASM_ERROR_EXECUTION_FAILED;
            break;
        default:
            last_result.error_code = WASM_ERROR_FUNCTION_NOT_FOUND;
            break;
        }
        
        wasm_status = WASM_STATUS_LOADED; /* Still loaded, just execution failed */
    }
    
    last_result_valid = true;
    return len;
}

/**
 * @brief Handle WASM status read requests
 */
static ssize_t wasm_status_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    wasm_status_packet_t status_packet;
    
    printk("WASM Service: Status read (status: %d, received: %u/%u bytes)\n",
           wasm_status, wasm_bytes_received, wasm_total_expected);
    
    status_packet.status = wasm_status;
    status_packet.error_code = wasm_error_code;
    status_packet.bytes_received = wasm_bytes_received;
    status_packet.total_size = wasm_total_expected;
    status_packet.uptime = k_uptime_get() / 1000;
    memset(status_packet.reserved, 0, sizeof(status_packet.reserved));
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &status_packet, sizeof(status_packet));
}

/**
 * @brief Handle WASM result read requests
 */
static ssize_t wasm_result_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    wasm_result_packet_t result_packet;
    
    printk("WASM Service: Result read request\n");
    
    if (last_result_valid) {
        result_packet = last_result;
        printk("WASM Service: Returning result (status: %d, value: %d)\n",
               result_packet.status, result_packet.return_value);
    } else {
        memset(&result_packet, 0, sizeof(result_packet));
        result_packet.status = WASM_STATUS_IDLE;
        result_packet.error_code = WASM_ERROR_NONE;
    }
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &result_packet, sizeof(result_packet));
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(wasm_service,
    BT_GATT_PRIMARY_SERVICE(WASM_SERVICE_UUID),
    
    /* WASM Upload Characteristic - Write for uploading WASM bytecode */
    BT_GATT_CHARACTERISTIC(WASM_UPLOAD_UUID,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, wasm_upload_write, NULL),
    
    /* WASM Execute Characteristic - Write for executing functions */
    BT_GATT_CHARACTERISTIC(WASM_EXECUTE_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, wasm_execute_write, NULL),
    
    /* WASM Status Characteristic - Read/Notify for status updates */
    BT_GATT_CHARACTERISTIC(WASM_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          wasm_status_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    
    /* WASM Result Characteristic - Read/Notify for execution results */
    BT_GATT_CHARACTERISTIC(WASM_RESULT_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          wasm_result_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int wasm_service_init(void)
{
    reset_upload_state();
    wasm_conn = NULL;
    
    /* Don't initialize WASM3 runtime yet - do it on first use */
    wasm_runtime_initialized = false;
    
    printk("WASM Service: Initialized\n");
    printk("  Upload characteristic: WRITE + WRITE_WITHOUT_RESP\n");
    printk("  Execute characteristic: WRITE\n");
    printk("  Status characteristic: READ + NOTIFY\n");
    printk("  Result characteristic: READ + NOTIFY\n");
    printk("  Code buffer size: %d bytes\n", WASM_CODE_BUFFER_SIZE);
    printk("  Upload chunk size: %d bytes\n", WASM_UPLOAD_CHUNK_SIZE);
    
    return 0;
}

void wasm_service_connection_event(struct bt_conn *conn, bool connected)
{
    if (connected) {
        printk("WASM Service: Client connected\n");
        wasm_conn = conn;
    } else {
        printk("WASM Service: Client disconnected\n");
        if (conn == wasm_conn) {
            wasm_conn = NULL;
            /* Optionally reset state on disconnect */
            /* reset_upload_state(); */
        }
    }
}

uint8_t wasm_service_get_status(void)
{
    return wasm_status;
}

uint8_t wasm_service_get_error_code(void)
{
    return wasm_error_code;
}

uint16_t wasm_service_get_bytes_received(void)
{
    return wasm_bytes_received;
}

bool wasm_service_is_ready(void)
{
    return (wasm_status == WASM_STATUS_LOADED && wasm_runtime_initialized);
}

void wasm_service_reset(void)
{
    printk("WASM Service: Resetting state\n");
    reset_upload_state();
    
    if (wasm_runtime_initialized) {
        wasm3_cleanup(&wasm_runtime);
        wasm_runtime_initialized = false;
    }
}

int wasm_service_execute_function(const char *function_name, 
                                 const int32_t *args, 
                                 uint32_t arg_count, 
                                 int32_t *result)
{
    if (!function_name || !wasm_service_is_ready()) {
        return -EINVAL;
    }
    
    printk("WASM Service: Direct execution of '%s'\n", function_name);
    
    int ret = wasm3_call_function(&wasm_runtime, function_name, args, arg_count, result);
    
    if (ret == WASM3_SUCCESS) {
        printk("WASM Service: Direct execution successful, result: %d\n", *result);
        return 0;
    } else {
        printk("WASM Service: Direct execution failed: %d\n", ret);
        return ret;
    }
}

int wasm_service_get_last_result(wasm_result_packet_t *result_packet)
{
    if (!result_packet) {
        return -EINVAL;
    }
    
    if (last_result_valid) {
        *result_packet = last_result;
        return 0;
    } else {
        memset(result_packet, 0, sizeof(*result_packet));
        result_packet->status = WASM_STATUS_IDLE;
        return -ENODATA;
    }
}

bool wasm_service_validate_magic(const uint8_t *data, size_t size)
{
    return validate_wasm_magic(data, size);
}
