#include "ble_services.h"
#include "device_info_service.h"
/* Re-enabling services with fixed UUID approach */
#include "control_service.h"
#include "data_service.h"
#include "dfu_service.h"
#include "sprite_service.h"
#include "wasm_service.h"
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>

/**
 * @file ble_services.c
 * @brief Common BLE services management and coordination
 */

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

static bool services_initialized = false;
static uint8_t active_connections = 0;
static uint16_t current_mtu = 23;  /* Default BLE MTU */

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int ble_services_init(void)
{
    int err;
    
    if (services_initialized) {
        printk("BLE Services: Already initialized\n");
        return 0;
    }
    
    printk("BLE Services: Initializing all services...\n");
    
    /* Initialize Device Information Service */
    printk("BLE Services: Initializing Device Information Service...\n");
    err = device_info_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Device Info Service (err %d)\n", err);
        return err;
    }
    printk("BLE Services: ✅ Device Information Service initialized\n");
    
    /* Re-enabling custom services with fixed UUID approach */
    printk("BLE Services: Initializing Control Service...\n");
    err = control_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Control Service (err %d)\n", err);
        return err;
    }
    printk("BLE Services: ✅ Control Service initialized\n");
    
    printk("BLE Services: Initializing Data Service...\n");
    err = data_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Data Service (err %d)\n", err);
        return err;
    }
    printk("BLE Services: ✅ Data Service initialized\n");
    
    printk("BLE Services: Initializing DFU Service...\n");
    err = dfu_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize DFU Service (err %d)\n", err);
        return err;
    }
    printk("BLE Services: ✅ DFU Service initialized\n");
    
    printk("BLE Services: Initializing Sprite Service...\n");
    err = sprite_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Sprite Service (err %d)\n", err);
        return err;
    }
    printk("BLE Services: ✅ Sprite Service initialized\n");
    
    printk("BLE Services: Initializing WASM Service...\n");
    err = wasm_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize WASM Service (err %d)\n", err);
        return err;
    }
    printk("BLE Services: ✅ WASM Service initialized\n");
    
    services_initialized = true;
    
    printk("BLE Services: All services initialized successfully\n");
            printk("BLE Services: Available services:\n");
        printk("  - Device Information Service (0x180A)\n");
        printk("  - Control Service (0xFFE0)\n");
        printk("  - Data Service (0xFFF0)\n");
        printk("  - DFU Service (0xFE59)\n");
        printk("  - Sprite Service (0xFFF8)\n");
        printk("  - WASM Service (0xFFF7)\n");
    
    return 0;
}

void ble_services_connection_event(struct bt_conn *conn, bool connected)
{
    if (!services_initialized) {
        return;
    }
    
    /* Update connection count */
    if (connected) {
        active_connections++;
        printk("BLE Services: 📱 New client connected! (active: %d)\n", active_connections);
        printk("BLE Services: Available services:\n");
        printk("  - Device Information Service (0x180A)\n");
        printk("  - Control Service (0xFFE0)\n");
        printk("  - Data Service (0xFFF0)\n");
        printk("  - DFU Service (0xFE59)\n");
    } else {
        if (active_connections > 0) {
            active_connections--;
        }
        printk("BLE Services: 📱 Client disconnected (active: %d)\n", active_connections);
    }
    
    printk("BLE Services: Connection event - %s (active: %d)\n", 
           connected ? "connected" : "disconnected", active_connections);
    
    /* Notify all services of connection events */
    control_service_connection_event(conn, connected);
    data_service_connection_event(conn, connected);
    dfu_service_connection_event(conn, connected);
    sprite_service_connection_event(conn, connected);
    wasm_service_connection_event(conn, connected);
}

uint8_t ble_services_get_device_status(void)
{
    if (!services_initialized) {
        return 0; // Not initialized
    }
    
    /* Return the control service device status as the overall status */
    return control_service_get_device_status();
}

uint8_t ble_services_get_connection_count(void)
{
    return active_connections;
}

bool ble_services_are_initialized(void)
{
    return services_initialized;
}

uint8_t ble_services_get_wasm_status(void)
{
    if (!services_initialized) {
        return 0; // Not initialized
    }
    
    /* Temporarily disabled
    return wasm_service_get_status();
    */
    return 0; // Service disabled
}

uint16_t ble_services_get_current_mtu(void)
{
    return current_mtu;
}

/* ============================================================================
 * MTU EXCHANGE CALLBACK
 * ============================================================================ */

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params)
{
    if (err) {
        printk("BLE Services: MTU exchange failed (err %d)\n", err);
        return;
    }
    
    current_mtu = bt_gatt_get_mtu(conn);
    printk("BLE Services: 🔄 MTU negotiated: %d bytes\n", current_mtu);
    printk("BLE Services: 📦 Max payload size: %d bytes\n", current_mtu - 3); /* ATT header is 3 bytes */
    
    /* Log what this enables */
    if (current_mtu >= 247) {
        printk("BLE Services: ✅ Large packet support enabled (244+ byte payloads)\n");
        printk("BLE Services: 🚀 WASM service can use full-size packets\n");
    } else if (current_mtu >= 50) {
        printk("BLE Services: ✅ Medium packet support enabled (%d byte payloads)\n", current_mtu - 3);
    } else {
        printk("BLE Services: ⚠️  Using minimum MTU - limited to %d byte payloads\n", current_mtu - 3);
    }
}

static struct bt_gatt_exchange_params mtu_exchange_params = {
    .func = mtu_exchange_cb
};

int ble_services_request_mtu_exchange(struct bt_conn *conn)
{
    if (!conn) {
        printk("BLE Services: Cannot request MTU exchange - no connection\n");
        return -EINVAL;
    }
    
    printk("BLE Services: 📡 Requesting MTU exchange...\n");
    return bt_gatt_exchange_mtu(conn, &mtu_exchange_params);
}
