#ifndef CONTROL_SERVICE_H
#define CONTROL_SERVICE_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <stdint.h>

/**
 * @file control_service.h
 * @brief Custom Control Service implementation
 * 
 * Provides device control interface with command/response pattern.
 * Follows industry standard BLE design with separate characteristics
 * for commands, responses, and status monitoring.
 */

/* ============================================================================
 * PACKET TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Control command packet structure
 * 
 * Used for sending commands to the control service.
 * Total size: 20 bytes
 */
typedef struct {
    uint8_t cmd_id;      ///< Command identifier (CMD_*)
    uint8_t param1;      ///< First parameter
    uint8_t param2;      ///< Second parameter  
    uint8_t reserved[17]; ///< Reserved for future use
} __attribute__((packed)) control_command_packet_t;

/**
 * @brief Control response packet structure
 * 
 * Used for receiving responses from the control service.
 * Total size: 8 bytes
 */
typedef struct {
    uint8_t cmd_id;      ///< Original command identifier
    uint8_t status;      ///< Response status (RESPONSE_*)
    uint8_t result[6];   ///< Response data
} __attribute__((packed)) control_response_packet_t;

/**
 * @brief Control status packet structure
 * 
 * Used for reading device status information.
 * Total size: 8 bytes
 */
typedef struct {
    uint8_t device_status; ///< Current device status (DEVICE_STATUS_*)
    uint32_t uptime;       ///< Device uptime in seconds
    uint8_t reserved[3];   ///< Reserved for future use
} __attribute__((packed)) control_status_packet_t;

/* ============================================================================
 * CONTROL SERVICE DEFINITIONS
 * ============================================================================ */

/* Temporary 16-bit UUIDs to avoid macro conflicts - TODO: fix UUID system */
#define CONTROL_SERVICE_UUID        BT_UUID_16(0xFFE0)
#define CONTROL_COMMAND_UUID        BT_UUID_16(0xFFE1)
#define CONTROL_RESPONSE_UUID       BT_UUID_16(0xFFE2)
#define CONTROL_STATUS_UUID         BT_UUID_16(0xFFE3)

/* ============================================================================
 * CONTROL COMMANDS
 * ============================================================================ */

#define CMD_GET_STATUS              0x01
#define CMD_RESET_DEVICE            0x02
#define CMD_SET_CONFIG              0x03
#define CMD_GET_VERSION             0x04

/* ============================================================================
 * DEVICE STATUS CODES
 * ============================================================================ */

#define DEVICE_STATUS_IDLE          0x00
#define DEVICE_STATUS_BUSY          0x01
#define DEVICE_STATUS_ERROR         0x02

/* ============================================================================
 * RESPONSE CODES
 * ============================================================================ */

#define RESPONSE_SUCCESS            0x00
#define RESPONSE_ERROR_INVALID_DATA 0x01
#define RESPONSE_ERROR_UNKNOWN_CMD  0xFF

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize Control Service
 * 
 * Registers the Control Service with command, response, and status
 * characteristics for device control operations.
 * 
 * @return 0 on success, negative error code on failure
 */
int control_service_init(void);

/**
 * @brief Handle connection events for Control Service
 * 
 * Manages service state when clients connect/disconnect.
 * 
 * @param conn Connection handle
 * @param connected True if connected, false if disconnected
 */
void control_service_connection_event(struct bt_conn *conn, bool connected);

/**
 * @brief Get current device status
 * @return Current device status (DEVICE_STATUS_*)
 */
uint8_t control_service_get_device_status(void);

/**
 * @brief Set device status
 * @param status New device status (DEVICE_STATUS_*)
 */
void control_service_set_device_status(uint8_t status);

/**
 * @brief Send asynchronous response to connected client
 * 
 * Sends a response via notification to the connected client.
 * Used for responses that don't directly correspond to a command.
 * 
 * @param response_data Response data buffer
 * @param length Length of response data
 * @return 0 on success, negative error code on failure
 */
int control_service_send_response(const uint8_t *response_data, uint16_t length);

/**
 * @brief Get last response data
 * @param buffer Buffer to copy response data to
 * @param max_length Maximum buffer size
 * @return Number of bytes copied, or negative error code
 */
int control_service_get_last_response(uint8_t *buffer, uint16_t max_length);

#endif /* CONTROL_SERVICE_H */
