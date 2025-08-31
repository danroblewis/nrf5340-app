#ifndef DATA_SERVICE_H
#define DATA_SERVICE_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <stdint.h>

/**
 * @file data_service.h
 * @brief Custom Data Service implementation
 * 
 * Provides data transfer interface with upload, download, and status
 * monitoring capabilities. Follows industry standard patterns for
 * BLE data transfer operations.
 */

/* ============================================================================
 * PACKET TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Data upload packet structure
 * 
 * Used for uploading data chunks to the device.
 * Total size: 20 bytes (maximum BLE packet size)
 */
typedef struct {
    uint8_t data[20];  ///< Data payload (up to 20 bytes)
} __attribute__((packed)) data_upload_packet_t;

/**
 * @brief Data download packet structure
 * 
 * Used for downloading data chunks from the device.
 * Total size: 20 bytes
 */
typedef struct {
    uint8_t data[20];  ///< Data payload (up to 20 bytes)
} __attribute__((packed)) data_download_packet_t;

/**
 * @brief Data transfer status packet structure
 * 
 * Used for monitoring transfer progress and status.
 * Total size: 6 bytes
 */
typedef struct {
    uint8_t transfer_status;  ///< Transfer status (TRANSFER_STATUS_*)
    uint16_t buffer_size;     ///< Current buffer size in bytes
    uint8_t reserved[3];      ///< Reserved for future use
} __attribute__((packed)) data_transfer_status_packet_t;

/* ============================================================================
 * DATA SERVICE DEFINITIONS
 * ============================================================================ */

/* Static UUID definitions to avoid macro conflicts */
static const struct bt_uuid_16 data_service_uuid = BT_UUID_INIT_16(0xFFF0);
static const struct bt_uuid_16 data_upload_uuid = BT_UUID_INIT_16(0xFFF1);
static const struct bt_uuid_16 data_download_uuid = BT_UUID_INIT_16(0xFFF2);
static const struct bt_uuid_16 data_transfer_status_uuid = BT_UUID_INIT_16(0xFFF3);

#define DATA_SERVICE_UUID           (&data_service_uuid.uuid)
#define DATA_UPLOAD_UUID            (&data_upload_uuid.uuid)
#define DATA_DOWNLOAD_UUID          (&data_download_uuid.uuid)
#define DATA_TRANSFER_STATUS_UUID   (&data_transfer_status_uuid.uuid)

/* ============================================================================
 * TRANSFER STATUS CODES
 * ============================================================================ */

#define TRANSFER_STATUS_IDLE        0x00
#define TRANSFER_STATUS_RECEIVING   0x01
#define TRANSFER_STATUS_COMPLETE    0x02
#define TRANSFER_STATUS_ERROR       0x03

/* ============================================================================
 * DATA BUFFER SIZE
 * ============================================================================ */

#define DATA_BUFFER_SIZE            1024

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize Data Service
 * 
 * Registers the Data Service with upload, download, and transfer status
 * characteristics for data transfer operations.
 * 
 * @return 0 on success, negative error code on failure
 */
int data_service_init(void);

/**
 * @brief Handle connection events for Data Service
 * 
 * Manages service state when clients connect/disconnect.
 * 
 * @param conn Connection handle
 * @param connected True if connected, false if disconnected
 */
void data_service_connection_event(struct bt_conn *conn, bool connected);

/**
 * @brief Get current transfer status
 * @return Current transfer status (TRANSFER_STATUS_*)
 */
uint8_t data_service_get_transfer_status(void);

/**
 * @brief Get number of bytes in data buffer
 * @return Number of bytes currently in buffer
 */
uint16_t data_service_get_buffer_size(void);

/**
 * @brief Get data from buffer
 * @param buffer Buffer to copy data to
 * @param max_length Maximum buffer size
 * @return Number of bytes copied, or negative error code
 */
int data_service_get_buffer_data(uint8_t *buffer, uint16_t max_length);

/**
 * @brief Clear data buffer and reset transfer status
 */
void data_service_clear_buffer(void);

/**
 * @brief Set download data for clients to read
 * @param data Data to make available for download
 * @param length Length of data
 * @return 0 on success, negative error code on failure
 */
int data_service_set_download_data(const uint8_t *data, uint16_t length);

/**
 * @brief Process received data
 * 
 * Called internally when upload is complete to process the received data.
 * Can be overridden by applications for custom data processing.
 * 
 * @param data Received data buffer
 * @param length Length of received data
 */
void data_service_process_data(const uint8_t *data, uint16_t length);

#endif /* DATA_SERVICE_H */
