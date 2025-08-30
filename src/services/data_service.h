#ifndef DATA_SERVICE_H
#define DATA_SERVICE_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

/**
 * @file data_service.h
 * @brief Custom Data Service implementation
 * 
 * Provides data transfer interface with upload, download, and status
 * monitoring capabilities. Follows industry standard patterns for
 * BLE data transfer operations.
 */

/* ============================================================================
 * DATA SERVICE DEFINITIONS
 * ============================================================================ */

#define DATA_SERVICE_UUID           BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ABCD))
#define DATA_UPLOAD_UUID            BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ACD0))
#define DATA_DOWNLOAD_UUID          BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ACD1))
#define DATA_TRANSFER_STATUS_UUID   BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ACD2))

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
