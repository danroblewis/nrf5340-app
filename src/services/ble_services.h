#ifndef BLE_SERVICES_H
#define BLE_SERVICES_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <stdbool.h>

/**
 * @file ble_services.h
 * @brief Common definitions and utilities for BLE services
 */

/* ============================================================================
 * COMMON BLE SERVICE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Initialize all BLE services
 * @return 0 on success, negative error code on failure
 */
int ble_services_init(void);

/**
 * @brief Handle BLE connection events for all services
 * @param conn Connection handle
 * @param connected True if connected, false if disconnected
 */
void ble_services_connection_event(struct bt_conn *conn, bool connected);

/**
 * @brief Get current device status across all services
 * @return Device status code
 */
uint8_t ble_services_get_device_status(void);

/**
 * @brief Get WASM service status
 * @return WASM service status code
 */
uint8_t ble_services_get_wasm_status(void);

/* ============================================================================
 * COMMON SERVICE UTILITIES
 * ============================================================================ */

/**
 * @brief Get the number of active BLE connections
 * @return Number of active connections
 */
uint8_t ble_services_get_connection_count(void);

/**
 * @brief Check if services are initialized
 * @return True if all services are initialized, false otherwise
 */
bool ble_services_are_initialized(void);

/**
 * @brief Get current negotiated MTU size
 * @return Current MTU in bytes
 */
uint16_t ble_services_get_current_mtu(void);

/**
 * @brief Request MTU exchange with connected client
 * @param conn Connection handle
 * @return 0 on success, negative error code on failure
 */
int ble_services_request_mtu_exchange(struct bt_conn *conn);

#endif /* BLE_SERVICES_H */
