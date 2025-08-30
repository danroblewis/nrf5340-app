#ifndef DFU_SERVICE_H
#define DFU_SERVICE_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <stdint.h>

/**
 * @file dfu_service.h
 * @brief Device Firmware Update Service (0xFE59) implementation
 * 
 * Mock implementation of Nordic's Device Firmware Update protocol.
 * Provides standard DFU interface for firmware updates over BLE.
 */

/* ============================================================================
 * PACKET TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief DFU control point packet structure
 * 
 * Used for sending DFU commands to the control point characteristic.
 * Total size: 20 bytes
 */
typedef struct {
    uint8_t command;      ///< DFU command opcode (DFU_CMD_*)
    uint8_t param[19];    ///< Command parameters (up to 19 bytes)
} __attribute__((packed)) dfu_control_packet_t;

/**
 * @brief DFU firmware data packet structure
 * 
 * Used for sending firmware data chunks to the packet characteristic.
 * Total size: 20 bytes
 */
typedef struct {
    uint8_t data[20];     ///< Firmware data chunk (up to 20 bytes)
} __attribute__((packed)) dfu_packet_t;

/* ============================================================================
 * DFU SERVICE DEFINITIONS
 * ============================================================================ */

/* Use 16-bit UUIDs to avoid macro conflicts - DFU service keeps standard UUID */
#define DFU_SERVICE_UUID            BT_UUID_16(0xFE59)
#define DFU_CONTROL_POINT_UUID      BT_UUID_16(0xFFD0)
#define DFU_PACKET_UUID             BT_UUID_16(0xFFD1)

/* ============================================================================
 * DFU COMMANDS AND RESPONSES
 * ============================================================================ */

/* DFU Control Point Commands */
#define DFU_CMD_START_DFU           0x01
#define DFU_CMD_INITIALIZE_DFU      0x02
#define DFU_CMD_RECEIVE_FW          0x03
#define DFU_CMD_VALIDATE_FW         0x04
#define DFU_CMD_ACTIVATE_N_RESET    0x05

/* DFU Response Codes */
#define DFU_RSP_SUCCESS             0x01
#define DFU_RSP_INVALID_STATE       0x02
#define DFU_RSP_NOT_SUPPORTED       0x03
#define DFU_RSP_DATA_SIZE_EXCEEDS   0x04
#define DFU_RSP_CRC_ERROR           0x05
#define DFU_RSP_OPERATION_FAILED    0x06

/* DFU States */
#define DFU_STATE_IDLE              0x00
#define DFU_STATE_READY             0x01
#define DFU_STATE_RECEIVING         0x02

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize DFU Service
 * 
 * Registers the Device Firmware Update Service with control point
 * and packet characteristics for firmware update operations.
 * 
 * @return 0 on success, negative error code on failure
 */
int dfu_service_init(void);

/**
 * @brief Handle connection events for DFU Service
 * 
 * Manages DFU state when clients connect/disconnect.
 * 
 * @param conn Connection handle
 * @param connected True if connected, false if disconnected
 */
void dfu_service_connection_event(struct bt_conn *conn, bool connected);

/**
 * @brief Get current DFU state
 * @return Current DFU state (DFU_STATE_*)
 */
uint8_t dfu_service_get_state(void);

/**
 * @brief Get number of firmware bytes received
 * @return Number of bytes received in current transfer
 */
uint32_t dfu_service_get_bytes_received(void);

/**
 * @brief Reset DFU service to idle state
 */
void dfu_service_reset(void);

#endif /* DFU_SERVICE_H */
