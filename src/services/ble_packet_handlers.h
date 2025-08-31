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

/**
 * @brief Macros to generate clean wrapper functions
 * 
 * These generate the BLE boilerplate so you can write simple functions
 * that take your struct types directly.
 */

/* Generate a BLE write wrapper for a clean handler function */
#define BLE_WRITE_WRAPPER(handler_name, struct_type) \
    static ssize_t handler_name##_ble(struct bt_conn *conn, const struct bt_gatt_attr *attr, \
                                      const void *buf, uint16_t len, uint16_t offset, uint8_t flags) \
    { \
        if (len < sizeof(struct_type)) { \
            printk(#handler_name ": Packet too small (%d < %zu)\n", len, sizeof(struct_type)); \
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN); \
        } \
        const struct_type *packet = (const struct_type *)buf; \
        return handler_name(packet); \
    }

/* Generate a BLE write wrapper for variable-length data */
#define BLE_WRITE_WRAPPER_VARIABLE(handler_name, min_size, max_size) \
    static ssize_t handler_name##_ble(struct bt_conn *conn, const struct bt_gatt_attr *attr, \
                                      const void *buf, uint16_t len, uint16_t offset, uint8_t flags) \
    { \
        if (len < min_size) { \
            printk(#handler_name ": Packet too small (%d < %d)\n", len, min_size); \
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN); \
        } \
        if (len > max_size) { \
            printk(#handler_name ": Packet too large (%d > %d)\n", len, max_size); \
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN); \
        } \
        return handler_name(buf, len); \
    }

/* Generate a BLE read wrapper for a clean handler function */
#define BLE_READ_WRAPPER(handler_name, struct_type) \
    static ssize_t handler_name##_ble(struct bt_conn *conn, const struct bt_gatt_attr *attr, \
                                      void *buf, uint16_t len, uint16_t offset) \
    { \
        struct_type response; \
        ssize_t result = handler_name(&response); \
        if (result < 0) return result; \
        return bt_gatt_attr_read(conn, attr, buf, len, offset, &response, sizeof(response)); \
    }

/* Declare clean handler function signatures */
#define DECLARE_WRITE_HANDLER(handler_name, struct_type) \
    static ssize_t handler_name(const struct_type *packet)

#define DECLARE_READ_HANDLER(handler_name, struct_type) \
    static ssize_t handler_name(struct_type *response)

#endif /* BLE_PACKET_HANDLERS_H */
