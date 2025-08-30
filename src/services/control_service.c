#include "control_service.h"
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
static uint8_t last_response[64];
static uint16_t last_response_len = 0;
static struct bt_conn *control_conn = NULL;

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static void control_notify_response(void)
{
    if (!control_conn || last_response_len == 0) {
        return;
    }
    
    printk("Control Service: Notifying response (%d bytes)\n", last_response_len);
    /* In real implementation, would use bt_gatt_notify() */
}

/* ============================================================================
 * CHARACTERISTIC HANDLERS
 * ============================================================================ */

static ssize_t control_command_write(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags)
{
    const uint8_t *data = (const uint8_t *)buf;
    
    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    printk("Control Service: Command received: 0x%02x\n", data[0]);
    
    control_conn = conn;
    
    switch (data[0]) {
    case CMD_GET_STATUS:
        printk("Control Service: Get status command\n");
        last_response[0] = CMD_GET_STATUS;
        last_response[1] = RESPONSE_SUCCESS;
        last_response[2] = device_status;
        last_response_len = 3;
        control_notify_response();
        break;
        
    case CMD_RESET_DEVICE:
        printk("Control Service: Reset device command (mock)\n");
        device_status = DEVICE_STATUS_IDLE; // Reset to idle
        last_response[0] = CMD_RESET_DEVICE;
        last_response[1] = RESPONSE_SUCCESS;
        last_response_len = 2;
        control_notify_response();
        break;
        
    case CMD_SET_CONFIG:
        if (len >= 2) {
            printk("Control Service: Set config command (value: 0x%02x)\n", data[1]);
            last_response[0] = CMD_SET_CONFIG;
            last_response[1] = RESPONSE_SUCCESS;
            last_response_len = 2;
        } else {
            printk("Control Service: Set config command - insufficient data\n");
            last_response[0] = CMD_SET_CONFIG;
            last_response[1] = RESPONSE_ERROR_INVALID_DATA;
            last_response_len = 2;
        }
        control_notify_response();
        break;
        
    case CMD_GET_VERSION:
        printk("Control Service: Get version command\n");
        last_response[0] = CMD_GET_VERSION;
        last_response[1] = RESPONSE_SUCCESS;
        last_response[2] = 1; // Major
        last_response[3] = 0; // Minor
        last_response[4] = 0; // Patch
        last_response_len = 5;
        control_notify_response();
        break;
        
    default:
        printk("Control Service: Unknown command: 0x%02x\n", data[0]);
        last_response[0] = data[0];
        last_response[1] = RESPONSE_ERROR_UNKNOWN_CMD;
        last_response_len = 2;
        control_notify_response();
        break;
    }
    
    return len;
}

static ssize_t control_response_read(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    void *buf, uint16_t len, uint16_t offset)
{
    printk("Control Service: Response read request\n");
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           last_response, last_response_len);
}

static ssize_t control_status_read(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  void *buf, uint16_t len, uint16_t offset)
{
    uint8_t status_data[4] = {
        device_status,
        (uint8_t)(k_uptime_get() & 0xFF),
        (uint8_t)((k_uptime_get() >> 8) & 0xFF),
        (uint8_t)((k_uptime_get() >> 16) & 0xFF)
    };
    
    printk("Control Service: Status read request (status: %d)\n", device_status);
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           status_data, sizeof(status_data));
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(control_service,
    BT_GATT_PRIMARY_SERVICE(CONTROL_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(CONTROL_COMMAND_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, control_command_write, NULL),
    BT_GATT_CHARACTERISTIC(CONTROL_RESPONSE_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          control_response_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(CONTROL_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          control_status_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int control_service_init(void)
{
    device_status = DEVICE_STATUS_IDLE;
    last_response_len = 0;
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
    
    memcpy(last_response, response_data, length);
    last_response_len = length;
    
    control_notify_response();
    return 0;
}

int control_service_get_last_response(uint8_t *buffer, uint16_t max_length)
{
    if (!buffer || max_length == 0) {
        return -EINVAL;
    }
    
    uint16_t copy_len = (last_response_len < max_length) ? last_response_len : max_length;
    memcpy(buffer, last_response, copy_len);
    
    return copy_len;
}
