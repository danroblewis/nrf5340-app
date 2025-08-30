#include "data_service.h"
#include "ble_packet_handlers.h"
#include <zephyr/sys/printk.h>
#include <string.h>

/**
 * @file data_service.c
 * @brief Custom Data Service implementation
 */

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

static uint8_t data_buffer[DATA_BUFFER_SIZE];
static uint16_t data_buffer_size = 0;
static uint8_t transfer_status = TRANSFER_STATUS_IDLE;
static struct bt_conn *data_conn = NULL;

/* Static download data */
static const char *download_data = "Sample data from nRF5340 device";
static uint16_t download_data_length = 0;

/* ============================================================================
 * SIMPLIFIED CHARACTERISTIC HANDLERS
 * ============================================================================ */

// The macro will generate data_upload_write() wrapper that calls this
/**
 * @brief Handle data upload requests - CLEAN VERSION!
 * This function takes your struct directly, no BLE boilerplate needed.
 */
static ssize_t data_upload_handler(const data_upload_packet_t *packet)
{
    printk("Data Service: Upload received %zu bytes\n", sizeof(*packet));
    
    // Find actual data length (exclude padding zeros)
    uint16_t actual_len = sizeof(*packet);
    while (actual_len > 0 && packet->data[actual_len - 1] == 0) {
        actual_len--;
    }
    
    if (data_buffer_size + actual_len > DATA_BUFFER_SIZE) {
        printk("Data Service: Buffer overflow, resetting\n");
        data_buffer_size = 0;
        transfer_status = TRANSFER_STATUS_ERROR;
        return -1;
    }
    
    memcpy(data_buffer + data_buffer_size, packet->data, actual_len);
    data_buffer_size += actual_len;
    transfer_status = TRANSFER_STATUS_RECEIVING;
    
    printk("Data Service: Total received: %d bytes\n", data_buffer_size);
    
    /* Check for end marker or complete message */
    if (actual_len < sizeof(*packet)) { // Assume end of transmission if not full packet
        transfer_status = TRANSFER_STATUS_COMPLETE;
        printk("Data Service: Transfer complete\n");
        
        /* Process received data */
        data_service_process_data(data_buffer, data_buffer_size);
    }
    
    return sizeof(*packet);
}

// The macro will generate data_download_read() wrapper that calls this
/**
 * @brief Get data download - CLEAN VERSION!
 * This function fills your struct directly, no BLE boilerplate needed.
 */
static ssize_t data_download_handler(data_download_packet_t *response)
{
    printk("Data Service: Download request\n");
    
    if (download_data_length == 0) {
        download_data_length = strlen(download_data);
    }
    
    // For simplicity, just copy the sample data into the response
    uint16_t copy_len = (download_data_length < sizeof(response->data)) ? 
                        download_data_length : sizeof(response->data);
    
    memcpy(response->data, download_data, copy_len);
    
    // Pad with zeros if needed
    if (copy_len < sizeof(response->data)) {
        memset(&response->data[copy_len], 0, sizeof(response->data) - copy_len);
    }
    
    return sizeof(*response);
}

// The macro will generate data_transfer_status_read() wrapper that calls this  
/**
 * @brief Get data transfer status - CLEAN VERSION!
 * This function fills your struct directly, no BLE boilerplate needed.
 */
static ssize_t data_transfer_status_handler(data_transfer_status_packet_t *status)
{
    printk("Data Service: Transfer status read (status: %d, size: %d)\n", 
           transfer_status, data_buffer_size);
    
    status->transfer_status = transfer_status;
    status->buffer_size = data_buffer_size;
    memset(status->reserved, 0, sizeof(status->reserved));
    
    return sizeof(*status);
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

/* ============================================================================
 * CLEAN HANDLERS - WORK WITH STRUCTS DIRECTLY
 * ============================================================================ */

/* Declare the clean handlers we want to write */
DECLARE_WRITE_HANDLER(data_upload_handler, data_upload_packet_t);
DECLARE_READ_HANDLER(data_download_handler, data_download_packet_t);
DECLARE_READ_HANDLER(data_transfer_status_handler, data_transfer_status_packet_t);

/* Generate BLE wrappers automatically */
BLE_WRITE_WRAPPER(data_upload_handler, data_upload_packet_t)
BLE_READ_WRAPPER(data_download_handler, data_download_packet_t)
BLE_READ_WRAPPER(data_transfer_status_handler, data_transfer_status_packet_t)

BT_GATT_SERVICE_DEFINE(data_service,
    BT_GATT_PRIMARY_SERVICE(DATA_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(DATA_UPLOAD_UUID,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, data_upload_handler_ble, NULL),
    BT_GATT_CHARACTERISTIC(DATA_DOWNLOAD_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          data_download_handler_ble, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(DATA_TRANSFER_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          data_transfer_status_handler_ble, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int data_service_init(void)
{
    data_buffer_size = 0;
    transfer_status = TRANSFER_STATUS_IDLE;
    data_conn = NULL;
    download_data_length = strlen(download_data);
    
    printk("Data Service: Initialized\n");
    printk("  Upload characteristic: WRITE + WRITE_WITHOUT_RESP\n");
    printk("  Download characteristic: READ + NOTIFY\n");
    printk("  Transfer Status characteristic: READ + NOTIFY\n");
    printk("  Buffer size: %d bytes\n", DATA_BUFFER_SIZE);
    
    return 0;
}

void data_service_connection_event(struct bt_conn *conn, bool connected)
{
    if (connected) {
        printk("Data Service: Client connected\n");
        data_conn = conn;
    } else {
        printk("Data Service: Client disconnected\n");
        if (conn == data_conn) {
            data_conn = NULL;
            /* Optionally reset transfer state on disconnect */
            data_buffer_size = 0;
            transfer_status = TRANSFER_STATUS_IDLE;
        }
    }
}

uint8_t data_service_get_transfer_status(void)
{
    return transfer_status;
}

uint16_t data_service_get_buffer_size(void)
{
    return data_buffer_size;
}

int data_service_get_buffer_data(uint8_t *buffer, uint16_t max_length)
{
    if (!buffer || max_length == 0) {
        return -EINVAL;
    }
    
    uint16_t copy_len = (data_buffer_size < max_length) ? data_buffer_size : max_length;
    memcpy(buffer, data_buffer, copy_len);
    
    return copy_len;
}

void data_service_clear_buffer(void)
{
    data_buffer_size = 0;
    transfer_status = TRANSFER_STATUS_IDLE;
    printk("Data Service: Buffer cleared\n");
}

int data_service_set_download_data(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0) {
        return -EINVAL;
    }
    
    /* For this implementation, we just point to the provided data */
    /* In a real implementation, you might copy to a dedicated buffer */
    download_data = (const char *)data;
    download_data_length = length;
    
    printk("Data Service: Download data set (%d bytes)\n", length);
    return 0;
}

void data_service_process_data(const uint8_t *data, uint16_t length)
{
    printk("Data Service: Processing %d bytes of data\n", length);
    
    /* Mock processing - just echo first few bytes */
    if (length > 0) {
        printk("Data Service: First bytes: ");
        for (int i = 0; i < (length > 8 ? 8 : length); i++) {
            printk("%02x ", data[i]);
        }
        printk("\n");
    }
    
    /* Custom processing can be added here */
    /* For example: parse commands, store to flash, etc. */
}
