#ifndef BLE_PACKET_HANDLERS_H
#define BLE_PACKET_HANDLERS_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/printk.h>

/**
 * @file ble_packet_handlers.h
 * @brief Type-safe BLE GATT characteristic handlers
 * 
 * Provides a drop-in replacement for BT_GATT_CHARACTERISTIC that adds
 * type safety by automatically generating wrapper functions.
 */

/* ============================================================================
 * TYPED CHARACTERISTIC MACRO
 * ============================================================================ */

/* Helper macro to generate read wrapper and handler declaration */
#define _GATT_READ_TYPED(read_func, read_type) \
    static ssize_t read_func##_handler(struct bt_conn *conn, \
                                      const struct bt_gatt_attr *attr, \
                                      read_type *response, \
                                      uint16_t max_len, uint16_t offset); \
    static ssize_t read_func(struct bt_conn *conn, \
                            const struct bt_gatt_attr *attr, \
                            void *buf, uint16_t len, uint16_t offset) \
    { \
        read_type response; \
        ssize_t result = read_func##_handler(conn, attr, &response, len, offset); \
        if (result < 0) return result; \
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &response, result); \
    }

/* Helper macro to generate write wrapper and handler declaration */
#define _GATT_WRITE_TYPED(write_func, write_type) \
    static ssize_t write_func##_handler(struct bt_conn *conn, \
                                       const struct bt_gatt_attr *attr, \
                                       const write_type *packet, \
                                       uint16_t len, uint16_t offset, uint8_t flags); \
    static ssize_t write_func(struct bt_conn *conn, \
                             const struct bt_gatt_attr *attr, \
                             const void *buf, uint16_t len, \
                             uint16_t offset, uint8_t flags) \
    { \
        if (len < sizeof(write_type)) { \
            printk(#write_func ": Packet too small (%d < %zu)\n", len, sizeof(write_type)); \
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN); \
        } \
        const write_type *packet = (const write_type *)buf; \
        return write_func##_handler(conn, attr, packet, len, offset, flags); \
    }

/* Main macro - generates wrappers based on whether read/write types are provided */
#define BT_GATT_CHARACTERISTIC_TYPED(uuid, properties, permissions, read_func, write_func, user_data, read_type, write_type) \
    _GATT_READ_TYPED(read_func, read_type) \
    _GATT_WRITE_TYPED(write_func, write_type) \
    BT_GATT_CHARACTERISTIC(uuid, properties, permissions, read_func, write_func, user_data)

/* ============================================================================
 * SIMPLIFIED HANDLERS - NO BLE BOILERPLATE
 * ============================================================================ */

/* Helper macro to generate simple read wrapper that ignores BLE parameters */
#define _GATT_READ_SIMPLE(read_func, read_type) \
    static ssize_t simple_##read_func(read_type *response); \
    static ssize_t read_func##_handler(struct bt_conn *conn, \
                                      const struct bt_gatt_attr *attr, \
                                      read_type *response, \
                                      uint16_t max_len, uint16_t offset) \
    { \
        return simple_##read_func(response); \
    }

/* Helper macro to generate simple write wrapper that ignores BLE parameters */
#define _GATT_WRITE_SIMPLE(write_func, write_type) \
    static ssize_t simple_##write_func(const write_type *packet); \
    static ssize_t write_func##_handler(struct bt_conn *conn, \
                                       const struct bt_gatt_attr *attr, \
                                       const write_type *packet, \
                                       uint16_t len, uint16_t offset, uint8_t flags) \
    { \
        return simple_##write_func(packet); \
    }

/* Simplified characteristic macro - you just implement simple handlers */
#define BT_GATT_CHARACTERISTIC_SIMPLE(uuid, properties, permissions, read_func, write_func, user_data, read_type, write_type) \
    _GATT_READ_SIMPLE(read_func, read_type) \
    _GATT_WRITE_SIMPLE(write_func, write_type) \
    _GATT_READ_TYPED(read_func, read_type) \
    _GATT_WRITE_TYPED(write_func, write_type) \
    BT_GATT_CHARACTERISTIC(uuid, properties, permissions, read_func, write_func, user_data)

#endif /* BLE_PACKET_HANDLERS_H */
