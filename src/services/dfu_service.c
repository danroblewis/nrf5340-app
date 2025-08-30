#include "dfu_service.h"
#include "ble_packet_handlers.h"
#include <zephyr/sys/printk.h>

/**
 * @file dfu_service.c
 * @brief Device Firmware Update Service (0xFE59) implementation
 */

/* ============================================================================
 * PACKET TYPE DEFINITIONS
 * ============================================================================ */

typedef struct {
    uint8_t command;
    uint8_t param[19];  // Command parameters (up to 19 bytes)
} __attribute__((packed)) dfu_control_packet_t;

typedef struct {
    uint8_t data[20];   // Firmware data chunk
} __attribute__((packed)) dfu_packet_t;

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

static uint8_t dfu_state = DFU_STATE_IDLE;
static uint32_t dfu_bytes_received = 0;
static struct bt_conn *dfu_conn = NULL;

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static void dfu_control_point_indicate(uint8_t opcode, uint8_t response_code)
{
    if (!dfu_conn) {
        return;
    }
    
    uint8_t response[3] = {0x60, opcode, response_code}; // 0x60 = Response opcode
    
    printk("DFU Service: Sending indication - OpCode: 0x%02x, Response: 0x%02x\n", 
           opcode, response_code);
    
    /* In real implementation, would use bt_gatt_indicate() */
    /* For mock, we just print the response */
}

/* ============================================================================
 * SIMPLIFIED CHARACTERISTIC HANDLERS
 * ============================================================================ */

// The macro will generate dfu_control_point_write() wrapper that calls this
static ssize_t simple_dfu_control_point_write(const dfu_control_packet_t *packet)
{
    printk("DFU Service: Control Point command received: 0x%02x\n", packet->command);
    
    // Note: dfu_conn needs to be set via connection event handler
    
    switch (packet->command) {
    case DFU_CMD_START_DFU:
        printk("DFU Service: Start DFU command\n");
        dfu_state = DFU_STATE_READY;
        dfu_bytes_received = 0;
        dfu_control_point_indicate(DFU_CMD_START_DFU, DFU_RSP_SUCCESS);
        break;
        
    case DFU_CMD_INITIALIZE_DFU:
        printk("DFU Service: Initialize DFU command\n");
        dfu_control_point_indicate(
            DFU_CMD_INITIALIZE_DFU,
            (dfu_state == DFU_STATE_READY) ? DFU_RSP_SUCCESS : DFU_RSP_INVALID_STATE
        );
        break;
        
    case DFU_CMD_RECEIVE_FW:
        printk("DFU Service: Receive firmware command\n");
        dfu_state = DFU_STATE_RECEIVING;
        dfu_control_point_indicate(DFU_CMD_RECEIVE_FW, DFU_RSP_SUCCESS);
        break;
        
    case DFU_CMD_VALIDATE_FW:
        printk("DFU Service: Validate firmware command\n");
        printk("DFU Service: Mock validation - received %d bytes\n", dfu_bytes_received);
        dfu_control_point_indicate(DFU_CMD_VALIDATE_FW, DFU_RSP_SUCCESS);
        break;
        
    case DFU_CMD_ACTIVATE_N_RESET:
        printk("DFU Service: Activate and reset command (mock - not actually resetting)\n");
        dfu_state = DFU_STATE_IDLE;
        dfu_control_point_indicate(DFU_CMD_ACTIVATE_N_RESET, DFU_RSP_SUCCESS);
        break;
        
    default:
        printk("DFU Service: Unknown command: 0x%02x\n", packet->command);
        dfu_control_point_indicate(packet->command, DFU_RSP_NOT_SUPPORTED);
        break;
    }
    
    return sizeof(*packet);
}

// The macro will generate dfu_packet_write() wrapper that calls this
static ssize_t simple_dfu_packet_write(const dfu_packet_t *packet)
{
    if (dfu_state != DFU_STATE_RECEIVING) {
        printk("DFU Service: Packet received but not in receive state\n");
        return -1;  // Error
    }
    
    // Find actual data length (exclude padding zeros at end)
    uint16_t actual_len = sizeof(*packet);
    while (actual_len > 0 && packet->data[actual_len - 1] == 0) {
        actual_len--;
    }
    
    dfu_bytes_received += actual_len;
    printk("DFU Service: Firmware packet received: %d bytes (total: %d)\n", 
           actual_len, dfu_bytes_received);
    
    /* Mock processing - just count bytes */
    
    return sizeof(*packet);
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(dfu_service,
    BT_GATT_PRIMARY_SERVICE(DFU_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC_SIMPLE(DFU_CONTROL_POINT_UUID,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
                          BT_GATT_PERM_WRITE,
                          NULL, dfu_control_point_write, NULL,
                          void, dfu_control_packet_t),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC_SIMPLE(DFU_PACKET_UUID,
                          BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, dfu_packet_write, NULL,
                          void, dfu_packet_t),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int dfu_service_init(void)
{
    dfu_state = DFU_STATE_IDLE;
    dfu_bytes_received = 0;
    dfu_conn = NULL;
    
    printk("DFU Service: Initialized (mock implementation)\n");
    printk("  Service UUID: 0xFE59\n");
    printk("  Control Point: WRITE + INDICATE\n");
    printk("  Packet: WRITE_WITHOUT_RESP\n");
    
    return 0;
}

void dfu_service_connection_event(struct bt_conn *conn, bool connected)
{
    if (connected) {
        printk("DFU Service: Client connected\n");
        dfu_conn = conn;
    } else {
        printk("DFU Service: Client disconnected\n");
        if (conn == dfu_conn) {
            dfu_conn = NULL;
            dfu_state = DFU_STATE_IDLE;
            dfu_bytes_received = 0;
        }
    }
}

uint8_t dfu_service_get_state(void)
{
    return dfu_state;
}

uint32_t dfu_service_get_bytes_received(void)
{
    return dfu_bytes_received;
}

void dfu_service_reset(void)
{
    dfu_state = DFU_STATE_IDLE;
    dfu_bytes_received = 0;
    printk("DFU Service: Reset to idle state\n");
}
