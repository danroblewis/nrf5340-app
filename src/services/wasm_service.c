#include "wasm_service.h"
#include "ble_packet_handlers.h"
#include "ble_services.h"
#include <zephyr/sys/printk.h>
#include <zephyr/sys/crc.h>
#include <zephyr/kernel.h>
#include <string.h>

/* Direct WASM3 includes - no more wrapper */
#include "m3_core.h"
#include "m3_env.h"
#include "m3_info.h"
#include "m3_api_libc.h"

/* WASM3 error constants */
#define m3Err_none NULL

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

/* Notification state */
static bool wasm_status_notify_enabled = false;
static bool wasm_result_notify_enabled = false;

/* WASM3 runtime - direct WASM3 structures */
static IM3Environment wasm_env = NULL;
static IM3Runtime wasm_runtime = NULL;
static IM3Module wasm_module = NULL;
static bool wasm_runtime_initialized = false;

/* WASM3 now uses fixed heap (configured in CMakeLists.txt) */

/* Last execution result */
static wasm_result_packet_t last_result;
static bool last_result_valid = false;

/* ============================================================================
 * DEDICATED THREAD ARCHITECTURE
 * ============================================================================ */

/* Message types for the work queue */
typedef enum {
    WASM_MSG_LOAD_MODULE,
    WASM_MSG_EXECUTE_FUNCTION,
    WASM_MSG_RESET
} wasm_msg_type_t;

/* Work message structure */
typedef struct {
    wasm_msg_type_t type;
    union {
        struct {
            char function_name[WASM_FUNCTION_NAME_SIZE];
            uint32_t arg_count;
            int32_t args[4];
        } execute;
        /* load_module and reset don't need additional data */
    } data;
} wasm_work_msg_t;

/* Message queue for WASM processing requests */
#define WASM_MSGQ_MAX_MSGS 4
#define WASM_MSGQ_MSG_SIZE sizeof(wasm_work_msg_t)

K_MSGQ_DEFINE(wasm_work_queue, WASM_MSGQ_MSG_SIZE, WASM_MSGQ_MAX_MSGS, 4);

/* Dedicated WASM processing thread */
#define WASM_THREAD_STACK_SIZE (16 * 1024)  /* 16KB stack for heavy WASM processing */
#define WASM_THREAD_PRIORITY 5

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */
static void wasm_work_thread_entry(void *arg1, void *arg2, void *arg3);
static void notify_status_change(void);
static int load_wasm_module(void);
static int execute_wasm_function_internal(const char *function_name, uint32_t arg_count, int32_t *args);
static void reset_wasm_service_internal(void);
static void reset_upload_state(void);

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Dedicated WASM processing thread entry point
 * This thread handles heavy WASM operations outside of BLE callbacks
 */
static void wasm_work_thread_entry(void *arg1, void *arg2, void *arg3)
{
    wasm_work_msg_t msg;

    printk("WASM Service: Work thread started (stack: %d bytes)\n", WASM_THREAD_STACK_SIZE);

    while (1) {
        /* Wait for work message */
        if (k_msgq_get(&wasm_work_queue, &msg, K_FOREVER) == 0) {
            printk("WASM Service: Processing work message type: %d\n", msg.type);

            switch (msg.type) {
            case WASM_MSG_LOAD_MODULE:
                printk("WASM Service: Thread: Loading WASM module...\n");
                if (load_wasm_module() == 0) {
                    printk("WASM Service: Thread: WASM module loaded successfully\n");
                    wasm_status = WASM_STATUS_LOADED;
                    wasm_error_code = WASM_ERROR_NONE;
                } else {
                    printk("WASM Service: Thread: WASM module loading failed\n");
                    wasm_status = WASM_STATUS_ERROR;
                }
                notify_status_change();
                break;

            case WASM_MSG_EXECUTE_FUNCTION:
                printk("WASM Service: Thread: Executing function: %s\n", msg.data.execute.function_name);
                if (execute_wasm_function_internal(msg.data.execute.function_name,
                                                   msg.data.execute.arg_count,
                                                   msg.data.execute.args) == 0) {
                    printk("WASM Service: Thread: Function executed successfully\n");
                } else {
                    printk("WASM Service: Thread: Function execution failed\n");
                }
                break;

            case WASM_MSG_RESET:
                printk("WASM Service: Thread: Resetting WASM service...\n");
                reset_wasm_service_internal();
                break;

            default:
                printk("WASM Service: Thread: Unknown message type: %d\n", msg.type);
                break;
            }
        }
    }
}

/* Define the thread after the function is defined */
K_THREAD_DEFINE(wasm_work_thread, WASM_THREAD_STACK_SIZE, wasm_work_thread_entry,
                NULL, NULL, NULL, WASM_THREAD_PRIORITY, 0, 0);

/**
 * @brief Send status change notification if enabled
 */
static void notify_status_change(void)
{
    if (wasm_conn && wasm_status_notify_enabled) {
        wasm_status_packet_t status_packet = {
            .status = wasm_status,
            .error_code = wasm_error_code,
            .bytes_received = wasm_bytes_received,
            .total_size = wasm_total_expected,
            .uptime = k_uptime_get_32(),
            .reserved = {0}
        };
        
        printk("WASM Service: Notifying status change - Status: %d, Error: %d, Bytes: %d\n",
               wasm_status, wasm_error_code, wasm_bytes_received);
        
        // For now, just log the status change - we'll implement proper notifications later
        printk("WASM Service: Status change notification would be sent here\n");
    }
}

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
    

    
    /* Create WASM3 environment */
    wasm_env = m3_NewEnvironment();
    if (!wasm_env) {
        printk("WASM Service: Failed to create WASM3 environment\n");
        wasm_error_code = WASM_ERROR_LOAD_FAILED;
        return -1;
    }
    
    /* Create WASM3 runtime with 32KB stack for larger WASM modules */
    printk("WASM Service: Creating WASM3 runtime with stack size: %u bytes\n", 16384);
    printk("WASM Service: Environment pointer: 0x%08x\n", (uint32_t)wasm_env);
    
    wasm_runtime = m3_NewRuntime(wasm_env, 16384, NULL);  // Reduced to 16KB for memory constraints
    if (!wasm_runtime) {
        printk("WASM Service: Failed to create WASM3 runtime\n");
        printk("WASM Service: This usually means m3_Malloc failed for the stack allocation\n");
        printk("WASM Service: Check if d_m3FixedHeap is working correctly\n");
        m3_FreeEnvironment(wasm_env);
        wasm_env = NULL;
        wasm_error_code = WASM_ERROR_LOAD_FAILED;
        return -1;
    }
    
    /* Note: m3_LinkLibC will be called after module is loaded */
    
    wasm_runtime_initialized = true;
    printk("WASM Service: WASM3 runtime initialized successfully\n");
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
    
    printk("WASM Service: Using WASM3 fixed heap (64KB)\n");
    
    /* Validate WASM magic number */
    if (!validate_wasm_magic(wasm_code_buffer, wasm_code_size)) {
        printk("WASM Service: Invalid WASM magic number\n");
        printk("WASM Service: First 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
               wasm_code_buffer[0], wasm_code_buffer[1], wasm_code_buffer[2], wasm_code_buffer[3],
               wasm_code_buffer[4], wasm_code_buffer[5], wasm_code_buffer[6], wasm_code_buffer[7]);
        wasm_error_code = WASM_ERROR_INVALID_MAGIC;
        return -1;
    }
    
    printk("WASM Service: WASM magic validated, size=%u bytes\n", wasm_code_size);
    
    /* Parse WASM module using the SAME environment as runtime */
    printk("WASM Service: Starting WASM module parsing (size: %u bytes)\n", wasm_code_size);
    printk("WASM Service: Available stack: %u bytes\n", CONFIG_MAIN_STACK_SIZE);
    printk("WASM Service: WASM3 runtime stack: %u bytes\n", 16384);
    
    /* Validate WASM3 environment */
    if (wasm_env == NULL) {
        printk("WASM Service: ERROR - wasm_env is NULL!\n");
        return -1;
    }
    printk("WASM Service: WASM3 environment validated: 0x%08x\n", (uint32_t)wasm_env);
    
    /* Check stack pointer for context validation */
    printk("WASM Service: Stack pointer: 0x%08x\n", (uint32_t)__builtin_frame_address(0));
    
    M3Result result = m3_ParseModule(wasm_env, &wasm_module, wasm_code_buffer, wasm_code_size);
    if (result != m3Err_none) {
        printk("WASM Service: Failed to parse WASM module: %s\n", result);
        wasm_error_code = WASM_ERROR_PARSE_FAILED;
        return -1;
    }
    
    printk("WASM Service: WASM module parsed successfully\n");
    
    /* Load module into runtime */
    result = m3_LoadModule(wasm_runtime, wasm_module);
    if (result != m3Err_none) {
        printk("WASM Service: Failed to load WASM module: %s\n", result);
        wasm_error_code = WASM_ERROR_LOAD_FAILED;
        return -1;
    }
    
    printk("WASM Service: WASM module loaded successfully\n");
    
    /* Compile module */
    result = m3_CompileModule(wasm_module);
    if (result != m3Err_none) {
        printk("WASM Service: Failed to compile WASM module: %s\n", result);
        wasm_error_code = WASM_ERROR_COMPILE_FAILED;
        return -1;
    }
    
    printk("WASM Service: WASM module compiled successfully\n");
    return 0;
}

/**
 * @brief Internal function to execute WASM function (called by thread)
 */
static int execute_wasm_function_internal(const char *function_name, uint32_t arg_count, int32_t *args)
{
    printk("WASM Service: Thread executing function: %s with %u args\n", function_name, arg_count);
    
    /* Record start time */
    uint32_t start_time = k_uptime_get_32();
    
    /* Execute function using direct WASM3 calls */
    int32_t result_value = 0;
    int ret = -1;
    
    /* Find function by name */
    IM3Function function;
    M3Result find_result = m3_FindFunction(&function, wasm_runtime, function_name);
    if (find_result != m3Err_none) {
        printk("WASM Service: Function '%s' not found: %s\n", function_name, find_result);
        last_result.status = WASM_STATUS_ERROR;
        last_result.error_code = WASM_ERROR_FUNCTION_NOT_FOUND;
        last_result_valid = true;
        wasm_status = WASM_STATUS_LOADED;
        return -1;
    }
    
    /* Call function with proper argument handling */
    M3Result call_result;
    
    if (arg_count == 0) {
        /* No arguments - use CallV */
        printk("WASM Service: Calling function with no arguments\n");
        call_result = m3_CallV(function);
    } else if (arg_count <= 4) {
        /* Function with arguments - use Call with argument pointers */
        printk("WASM Service: Calling function with %u arguments: ", arg_count);
        for (uint32_t i = 0; i < arg_count; i++) {
            printk("%d ", args[i]);
        }
        printk("\n");
        
        /* Prepare argument pointers for WASM3 */
        const void* argptrs[4];
        for (uint32_t i = 0; i < arg_count; i++) {
            argptrs[i] = &args[i];
        }
        
        call_result = m3_Call(function, arg_count, argptrs);
    } else {
        printk("WASM Service: Too many arguments (%u > 4)\n", arg_count);
        last_result.status = WASM_STATUS_ERROR;
        last_result.error_code = WASM_ERROR_INVALID_PARAMS;
        last_result_valid = true;
        wasm_status = WASM_STATUS_LOADED;
        return -1;
    }
    
    if (call_result == m3Err_none) {
        /* Get return value if function returns something */
        uint32_t returnCount = m3_GetRetCount(function);
        if (returnCount > 0) {
            int32_t ret_val;
            const void* retptrs[1] = { &ret_val };
            call_result = m3_GetResults(function, 1, retptrs);
            if (call_result == m3Err_none) {
                result_value = ret_val;
                printk("WASM Service: Function returned: %d\n", result_value);
            } else {
                printk("WASM Service: Failed to get return value: %s\n", call_result);
            }
        }
        ret = 0; /* Success */
    } else {
        printk("WASM Service: Function call failed: %s\n", call_result);
    }
    
    /* Calculate execution time */
    uint32_t end_time = k_uptime_get_32();
    uint32_t execution_time_us = (end_time - start_time) * 1000; /* Convert to microseconds */
    
    /* Fill result packet */
    last_result.return_value = result_value;
    last_result.execution_time_us = execution_time_us;
    
    if (ret == 0) {
        printk("WASM Service: Function executed successfully, result: %d\n", result_value);
        last_result.status = WASM_STATUS_COMPLETE;
        last_result.error_code = WASM_ERROR_NONE;
        wasm_status = WASM_STATUS_LOADED; /* Ready for next execution */
    } else {
        printk("WASM Service: Function execution failed\n");
        last_result.status = WASM_STATUS_ERROR;
        wasm_error_code = WASM_ERROR_EXECUTION_FAILED;
        wasm_status = WASM_STATUS_LOADED; /* Still loaded, just execution failed */
    }
    
    last_result_valid = true;
    return ret;
}

/**
 * @brief Internal function to reset WASM service (called by thread)
 */
static void reset_wasm_service_internal(void)
{
    printk("WASM Service: Thread resetting service...\n");
    
    /* Reset upload state */
    reset_upload_state();
    
    /* Reset WASM3 runtime */
    if (wasm_module) {
        m3_FreeModule(wasm_module);
        wasm_module = NULL;
    }
    
    if (wasm_runtime) {
        m3_FreeRuntime(wasm_runtime);
        wasm_runtime = NULL;
    }
    
    if (wasm_env) {
        m3_FreeEnvironment(wasm_env);
        wasm_env = NULL;
    }
    
    wasm_runtime_initialized = false;
    wasm_status = WASM_STATUS_IDLE;
    wasm_error_code = WASM_ERROR_NONE;
    last_result_valid = false;
    
    printk("WASM Service: Service reset complete\n");
}

/**
 * @brief Reset WASM service (called by BLE handler)
 */
static void reset_wasm_service(void)
{
    printk("WASM Service: BLE handler requesting service reset\n");
    
    /* Queue reset to dedicated thread */
    wasm_work_msg_t reset_msg = {
        .type = WASM_MSG_RESET
    };
    
    if (k_msgq_put(&wasm_work_queue, &reset_msg, K_NO_WAIT) == 0) {
        printk("WASM Service: Reset request queued to work thread\n");
    } else {
        printk("WASM Service: ERROR - Failed to queue reset request\n");
    }
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
 * @brief Handle WASM upload packets with variable length support
 */
static ssize_t wasm_upload_handler(const void *data, uint16_t len)
{
    printk("\n=== WASM Service: wasm_upload_handler called ===\n");
    printk("WASM Service: Upload packet received (%d bytes)\n", len);
    
    if (len < 8) {  // Minimum packet size: cmd(1) + seq(1) + chunk_size(2) + total_size(4)
        printk("WASM Service: Upload packet too small (%d < 8)\n", len);
        return -1;
    }
    
    const wasm_upload_packet_t *packet = (const wasm_upload_packet_t *)data;
    
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
            return -1;
        }
        
        reset_upload_state();
        wasm_total_expected = packet->total_size;
        wasm_status = WASM_STATUS_RECEIVING;
        wasm_upload_sequence = 0;
        
        /* Notify status change */
        notify_status_change();
        
        /* Fall through to process data in first packet */
        __fallthrough;
        
    case WASM_CMD_CONTINUE_UPLOAD:
        if (wasm_status != WASM_STATUS_RECEIVING) {
            printk("WASM Service: Not in receiving state\n");
            wasm_error_code = WASM_ERROR_INVALID_PARAMS;
            return -1;
        }
        
        /* Verify sequence number */
        if (packet->sequence != wasm_upload_sequence) {
            printk("WASM Service: Sequence mismatch (expected %d, got %d)\n",
                   wasm_upload_sequence, packet->sequence);
            wasm_error_code = WASM_ERROR_INVALID_PARAMS;
            wasm_status = WASM_STATUS_ERROR;
            return -1;
        }
        
        /* Check buffer overflow */
        if (wasm_bytes_received + packet->chunk_size > WASM_CODE_BUFFER_SIZE ||
            wasm_bytes_received + packet->chunk_size > wasm_total_expected) {
            printk("WASM Service: Buffer overflow during upload\n");
            wasm_error_code = WASM_ERROR_BUFFER_OVERFLOW;
            wasm_status = WASM_STATUS_ERROR;
            return -1;
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
            printk("WASM Service: Upload complete (%u bytes), queuing module load...\n", wasm_code_size);
            
            /* Notify status change */
            notify_status_change();
            
            /* Debug: Print first few bytes of received WASM */
            printk("WASM Service: First 16 bytes received:");
            for (int i = 0; i < 16 && i < wasm_code_size; i++) {
                printk(" %02x", wasm_code_buffer[i]);
            }
            printk("\n");
            
            /* Queue module loading to dedicated thread instead of doing it here */
            wasm_work_msg_t load_msg = {
                .type = WASM_MSG_LOAD_MODULE
            };
            
            if (k_msgq_put(&wasm_work_queue, &load_msg, K_NO_WAIT) == 0) {
                printk("WASM Service: Module load queued to work thread\n");
            } else {
                printk("WASM Service: ERROR - Failed to queue module load\n");
                wasm_status = WASM_STATUS_ERROR;
                wasm_error_code = WASM_ERROR_LOAD_FAILED;
                notify_status_change();
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
        printk("WASM Service: Reset command received\n");
        reset_wasm_service();
        return sizeof(*packet);
        
    default:
        printk("WASM Service: Unknown upload command: 0x%02x\n", packet->cmd);
        wasm_error_code = WASM_ERROR_INVALID_PARAMS;
        return -1;
    }
    
    return len;
}

/**
 * @brief Handle WASM execution requests
 */
static ssize_t wasm_execute_handler(const wasm_execute_packet_t *packet)
{
    printk("\n=== WASM Service: wasm_execute_handler called ===\n");
    
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
        return sizeof(*packet);
    }
    
    /* Validate function name */
    if (strnlen(packet->function_name, WASM_FUNCTION_NAME_SIZE) >= WASM_FUNCTION_NAME_SIZE) {
        printk("WASM Service: Function name too long\n");
        last_result.status = WASM_STATUS_ERROR;
        last_result.error_code = WASM_ERROR_INVALID_PARAMS;
        last_result_valid = true;
        return sizeof(*packet);
    }
    
    /* Queue function execution to dedicated thread instead of doing it here */
    
    wasm_work_msg_t exec_msg = {
        .type = WASM_MSG_EXECUTE_FUNCTION
    };
    
    /* Copy function name and arguments */
    strncpy(exec_msg.data.execute.function_name, packet->function_name, WASM_FUNCTION_NAME_SIZE - 1);
    exec_msg.data.execute.function_name[WASM_FUNCTION_NAME_SIZE - 1] = '\0';
    exec_msg.data.execute.arg_count = packet->arg_count;
    for (uint32_t i = 0; i < 4; i++) {
        exec_msg.data.execute.args[i] = (i < packet->arg_count) ? packet->args[i] : 0;
    }
    
    if (k_msgq_put(&wasm_work_queue, &exec_msg, K_NO_WAIT) == 0) {
        printk("WASM Service: Function execution queued to work thread\n");
        wasm_status = WASM_STATUS_EXECUTING;
    } else {
        printk("WASM Service: ERROR - Failed to queue function execution\n");
        wasm_status = WASM_STATUS_ERROR;
        wasm_error_code = WASM_ERROR_EXECUTION_FAILED;
        last_result.status = WASM_STATUS_ERROR;
        last_result.error_code = WASM_ERROR_EXECUTION_FAILED;
        last_result_valid = true;
    }
    
    return sizeof(*packet);
}

/**
 * @brief Handle WASM status read requests
 */
static ssize_t wasm_status_handler(wasm_status_packet_t *response)
{
    printk("\n=== WASM Service: wasm_status_handler called ===\n");
    printk("WASM Service: Status read (status: %d, received: %u/%u bytes)\n",
           wasm_status, wasm_bytes_received, wasm_total_expected);
    
    response->status = wasm_status;
    response->error_code = wasm_error_code;
    response->bytes_received = wasm_bytes_received;
    response->total_size = wasm_total_expected;
    response->uptime = k_uptime_get() / 1000;
    memset(response->reserved, 0, sizeof(response->reserved));
    
    return sizeof(*response);
}

/**
 * @brief Handle WASM result read requests
 */
static ssize_t wasm_result_handler(wasm_result_packet_t *response)
{
    printk("\n=== WASM Service: wasm_result_handler called ===\n");
    printk("WASM Service: Result read request\n");
    
    if (last_result_valid) {
        *response = last_result;
        printk("WASM Service: Returning result (status: %d, value: %d)\n",
               response->status, response->return_value);
    } else {
        memset(response, 0, sizeof(*response));
        response->status = WASM_STATUS_IDLE;
        response->error_code = WASM_ERROR_NONE;
    }
    
    return sizeof(*response);
}

/* ============================================================================
 * CCC HANDLERS
 * ============================================================================ */

static void wasm_status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    wasm_status_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    printk("WASM Service: Status notifications %s\n", 
           wasm_status_notify_enabled ? "enabled" : "disabled");
}

static void wasm_result_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    wasm_result_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    printk("WASM Service: Result notifications %s\n", 
           wasm_result_notify_enabled ? "enabled" : "disabled");
}

/* ============================================================================
 * BLE WRAPPER GENERATION
 * ============================================================================ */

/* Generate BLE wrappers automatically */
BLE_WRITE_WRAPPER_VARIABLE(wasm_upload_handler, 8, 252)  /* Variable length WASM upload packets - limited by BLE MTU */
BLE_WRITE_WRAPPER(wasm_execute_handler, wasm_execute_packet_t)
BLE_READ_WRAPPER(wasm_status_handler, wasm_status_packet_t)
BLE_READ_WRAPPER(wasm_result_handler, wasm_result_packet_t)

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(wasm_service,
    BT_GATT_PRIMARY_SERVICE(WASM_SERVICE_UUID),
    
    /* WASM Upload Characteristic - Write for uploading WASM bytecode */
    BT_GATT_CHARACTERISTIC(WASM_UPLOAD_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, wasm_upload_handler_ble, NULL),
    
    /* WASM Execute Characteristic - Write for executing functions */
    BT_GATT_CHARACTERISTIC(WASM_EXECUTE_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, wasm_execute_handler_ble, NULL),
    
    /* WASM Status Characteristic - Read/Notify for status updates */
    BT_GATT_CHARACTERISTIC(WASM_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          wasm_status_handler_ble, NULL, NULL),
    BT_GATT_CCC(wasm_status_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    
    /* WASM Result Characteristic - Read/Notify for execution results */
    BT_GATT_CHARACTERISTIC(WASM_RESULT_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          wasm_result_handler_ble, NULL, NULL),
    BT_GATT_CCC(wasm_result_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
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
    printk("  Upload characteristic: WRITE\n");
    printk("  Execute characteristic: WRITE\n");
    printk("  Status characteristic: READ + NOTIFY\n");
    printk("  Result characteristic: READ + NOTIFY\n");
    printk("  Code buffer size: %d bytes\n", WASM_CODE_BUFFER_SIZE);
    printk("  Upload chunk size: %d bytes\n", WASM_UPLOAD_CHUNK_SIZE);
    printk("  WASM3 runtime stack: 16KB\n");
    printk("  WASM3 fixed heap: 64KB\n");
    printk("  Status notifications: enabled\n");
    printk("  Result notifications: enabled\n");
    
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
    return (wasm_status == WASM_STATUS_LOADED && wasm_runtime_initialized && wasm_module != NULL);
}

void wasm_service_reset(void)
{
    printk("WASM Service: Resetting state\n");
    reset_upload_state();
    
    if (wasm_runtime_initialized) {
        /* Clean up WASM3 structures */
        if (wasm_module) {
            m3_FreeModule(wasm_module);
            wasm_module = NULL;
        }
        if (wasm_runtime) {
            m3_FreeRuntime(wasm_runtime);
            wasm_runtime = NULL;
        }
        if (wasm_env) {
            m3_FreeEnvironment(wasm_env);
            wasm_env = NULL;
        }
        wasm_runtime_initialized = false;
        printk("WASM Service: Runtime cleaned up\n");
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
    
    /* Find function by name */
    IM3Function function;
    M3Result find_result = m3_FindFunction(&function, wasm_runtime, function_name);
    if (find_result != m3Err_none) {
        printk("WASM Service: Function '%s' not found: %s\n", function_name, find_result);
        return -ENOENT;
    }
    
    /* Call function with proper argument handling */
    M3Result call_result;
    
    if (arg_count == 0) {
        call_result = m3_CallV(function);
    } else if (arg_count <= 4) {
        /* Prepare argument pointers */
        const void* argptrs[4];
        for (uint32_t i = 0; i < arg_count; i++) {
            argptrs[i] = &args[i];
        }
        call_result = m3_Call(function, arg_count, argptrs);
    } else {
        printk("WASM Service: Too many arguments (%u > 4)\n", arg_count);
        return -EINVAL;
    }
    
    if (call_result != m3Err_none) {
        printk("WASM Service: Direct execution failed: %s\n", call_result);
        return -EIO;
    }
    
    /* Get return value if requested */
    if (result) {
        uint32_t returnCount = m3_GetRetCount(function);
        if (returnCount > 0) {
            int32_t ret_val;
            const void* retptrs[1] = { &ret_val };
            call_result = m3_GetResults(function, 1, retptrs);
            if (call_result == m3Err_none) {
                *result = ret_val;
                printk("WASM Service: Direct execution successful, result: %d\n", *result);
            }
        }
    }
    
    return 0;
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
