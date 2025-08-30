#include "data_service.h"
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
 * CHARACTERISTIC HANDLERS
 * ============================================================================ */

static ssize_t data_upload_write(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len,
                                uint16_t offset, uint8_t flags)
{
    const uint8_t *data = (const uint8_t *)buf;
    
    printk("Data Service: Upload received %d bytes\n", len);
    
    if (data_buffer_size + len > DATA_BUFFER_SIZE) {
        printk("Data Service: Buffer overflow, resetting\n");
        data_buffer_size = 0;
        transfer_status = TRANSFER_STATUS_ERROR;
        return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }
    
    memcpy(data_buffer + data_buffer_size, data, len);
    data_buffer_size += len;
    transfer_status = TRANSFER_STATUS_RECEIVING;
    
    printk("Data Service: Total received: %d bytes\n", data_buffer_size);
    
    /* Check for end marker or complete message */
    if (len < 20) { // Assume end of transmission if less than MTU
        transfer_status = TRANSFER_STATUS_COMPLETE;
        printk("Data Service: Transfer complete\n");
        
        /* Process received data */
        data_service_process_data(data_buffer, data_buffer_size);
    }
    
    return len;
}

static ssize_t data_download_read(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
    printk("Data Service: Download request (offset: %d, len: %d)\n", offset, len);
    
    if (download_data_length == 0) {
        download_data_length = strlen(download_data);
    }
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           download_data, download_data_length);
}

static ssize_t data_transfer_status_read(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        void *buf, uint16_t len, uint16_t offset)
{
    uint8_t status_data[6] = {
        transfer_status,
        (uint8_t)(data_buffer_size & 0xFF),
        (uint8_t)((data_buffer_size >> 8) & 0xFF),
        0x00, // Reserved
        0x00, // Reserved
        0x00  // Reserved
    };
    
    printk("Data Service: Transfer status read (status: %d, size: %d)\n", 
           transfer_status, data_buffer_size);
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           status_data, sizeof(status_data));
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(data_service,
    BT_GATT_PRIMARY_SERVICE(DATA_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(DATA_UPLOAD_UUID,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, data_upload_write, NULL),
    BT_GATT_CHARACTERISTIC(DATA_DOWNLOAD_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          data_download_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(DATA_TRANSFER_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          data_transfer_status_read, NULL, NULL),
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
