#include "control_service.h"
#include "ble_packet_handlers.h"
#include <zephyr/sys/printk.h>
#include <string.h>

/**
 * @file control_service.c
 * @brief Custom Control Service implementation
 */

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

static uint8_t device_status = DEVICE_STATUS_IDLE;
static control_response_packet_t last_response;
static bool last_response_valid = false;
static struct bt_conn *control_conn = NULL;

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static void control_notify_response(void)
{
    if (!control_conn || !last_response_valid) {
        return;
    }
    
    printk("Control Service: Notifying response (cmd: 0x%02x, status: 0x%02x)\n", 
           last_response.cmd_id, last_response.status);
    /* In real implementation, would use bt_gatt_notify() */
}

/* ============================================================================
 * SIMPLIFIED CHARACTERISTIC HANDLERS
 * ============================================================================ */

// The macro will generate control_command_write() wrapper that calls this
static ssize_t simple_control_command_write(const control_command_packet_t *packet)
{
    printk("Control Service: Command received: 0x%02x\n", packet->cmd_id);
    
    last_response.cmd_id = packet->cmd_id;
    last_response.status = RESPONSE_SUCCESS;

    switch (packet->cmd_id) {
    case CMD_GET_STATUS:
        printk("Control Service: Get status (param1: 0x%02x)\n", packet->param1);
        last_response.result[0] = device_status;
        memset(&last_response.result[1], 0, sizeof(last_response.result) - 1);
        break;
        
    case CMD_RESET_DEVICE:
        printk("Control Service: Reset device command (mock)\n");
        device_status = DEVICE_STATUS_IDLE;
        memset(last_response.result, 0, sizeof(last_response.result));
        break;
        
    case CMD_SET_CONFIG:
        printk("Control Service: Set config (value: 0x%02x)\n", packet->param1);
        memset(last_response.result, 0, sizeof(last_response.result));
        break;
        
    case CMD_GET_VERSION:
        printk("Control Service: Get version command\n");
        last_response.result[0] = 1; // Major
        last_response.result[1] = 0; // Minor
        last_response.result[2] = 0; // Patch
        memset(&last_response.result[3], 0, sizeof(last_response.result) - 3);
        break;
        
    default:
        printk("Control Service: Unknown command: 0x%02x\n", packet->cmd_id);
        last_response.status = RESPONSE_ERROR_UNKNOWN_CMD;
        memset(last_response.result, 0, sizeof(last_response.result));
        break;
    }

    last_response_valid = true;
    control_notify_response();
    
    return sizeof(*packet);
}

// The macro will generate control_response_read() wrapper that calls this
static ssize_t simple_control_response_read(control_response_packet_t *response)
{
    printk("Control Service: Response read request\n");
    
    if (last_response_valid) {
        // Copy the typed response struct
        *response = last_response;
        return sizeof(*response);
    }
    
    // Default empty response
    memset(response, 0, sizeof(*response));
    return sizeof(*response);
}

// The macro will generate control_status_read() wrapper that calls this  
static ssize_t simple_control_status_read(control_status_packet_t *status)
{
    printk("Control Service: Status read request (status: %d)\n", device_status);
    
    status->device_status = device_status;
    status->uptime = k_uptime_get() / 1000;
    memset(status->reserved, 0, sizeof(status->reserved));
    
    return sizeof(*status);
}



/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(control_service,
    BT_GATT_PRIMARY_SERVICE(CONTROL_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC_SIMPLE(CONTROL_COMMAND_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, control_command_write, NULL, 
                          void, control_command_packet_t),
    BT_GATT_CHARACTERISTIC_SIMPLE(CONTROL_RESPONSE_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          control_response_read, NULL, NULL, 
                          control_response_packet_t, void),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC_SIMPLE(CONTROL_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          control_status_read, NULL, NULL,
                          control_status_packet_t, void),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int control_service_init(void)
{
    device_status = DEVICE_STATUS_IDLE;
    last_response_valid = false;
    memset(&last_response, 0, sizeof(last_response));
    control_conn = NULL;
    
    printk("Control Service: Initialized\n");
    printk("  Command characteristic: WRITE\n");
    printk("  Response characteristic: READ + NOTIFY\n");
    printk("  Status characteristic: READ + NOTIFY\n");
    
    return 0;
}

void control_service_connection_event(struct bt_conn *conn, bool connected)
{
    if (connected) {
        printk("Control Service: Client connected\n");
        control_conn = conn;
        device_status = DEVICE_STATUS_BUSY; // Device is now busy (connected)
    } else {
        printk("Control Service: Client disconnected\n");
        if (conn == control_conn) {
            control_conn = NULL;
            device_status = DEVICE_STATUS_IDLE; // Device is now idle
        }
    }
}

uint8_t control_service_get_device_status(void)
{
    return device_status;
}

void control_service_set_device_status(uint8_t status)
{
    if (status != device_status) {
        printk("Control Service: Device status changed from %d to %d\n", 
               device_status, status);
        device_status = status;
    }
}

int control_service_send_response(const uint8_t *response_data, uint16_t length)
{
    if (!response_data || length == 0 || length > sizeof(last_response)) {
        return -EINVAL;
    }
    
    // For backwards compatibility, copy raw data into response struct
    if (length >= 2) {
        last_response.cmd_id = response_data[0];
        last_response.status = response_data[1];
        
        // Copy remaining data into result field
        uint16_t result_len = length - 2;
        if (result_len > sizeof(last_response.result)) {
            result_len = sizeof(last_response.result);
        }
        memcpy(last_response.result, &response_data[2], result_len);
        
        // Clear remaining result bytes
        if (result_len < sizeof(last_response.result)) {
            memset(&last_response.result[result_len], 0, sizeof(last_response.result) - result_len);
        }
        
        last_response_valid = true;
        control_notify_response();
        return 0;
    }
    
    return -EINVAL;
}

int control_service_get_last_response(uint8_t *buffer, uint16_t max_length)
{
    if (!buffer || max_length == 0 || !last_response_valid) {
        return -EINVAL;
    }
    
    uint16_t copy_len = (sizeof(last_response) < max_length) ? sizeof(last_response) : max_length;
    memcpy(buffer, &last_response, copy_len);
    
    return copy_len;
}
