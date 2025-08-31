#include "ble_services.h"
#include "device_info_service.h"
/* Re-enabling services with fixed UUID approach */
#include "control_service.h"
#include "data_service.h"
#include "dfu_service.h"
/* Keep WASM disabled for now
#include "wasm_service.h"
*/
#include <zephyr/sys/printk.h>

/**
 * @file ble_services.c
 * @brief Common BLE services management and coordination
 */

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

static bool services_initialized = false;
static uint8_t active_connections = 0;

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
    
    /* Initialize WASM Service */
    /* Temporarily disabled due to BLE macro issues
    err = wasm_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize WASM Service (err %d)\n", err);
        return err;
    }
    */
    
    services_initialized = true;
    
    printk("BLE Services: All services initialized successfully\n");
    printk("BLE Services: Available services:\n");
    printk("  - Device Information Service (0x180A)\n");
    printk("  - Control Service (0xFFE0)\n");
    printk("  - Data Service (0xFFF0)\n");
    printk("  - DFU Service (0xFE59)\n");
    printk("BLE Services: Note - WASM service disabled for now\n");
    
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
    /* wasm_service_connection_event(conn, connected); // WASM disabled for now */
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
