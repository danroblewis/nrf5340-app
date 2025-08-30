#include "ble_services.h"
#include "device_info_service.h"
/* Temporarily disabled due to Zephyr BLE macro conflicts
#include "dfu_service.h"
#include "control_service.h"
#include "data_service.h"
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
    err = device_info_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Device Info Service (err %d)\n", err);
        return err;
    }
    
    /* Temporarily disable custom services due to Zephyr BLE macro conflicts
    err = dfu_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize DFU Service (err %d)\n", err);
        return err;
    }
    
    err = control_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Control Service (err %d)\n", err);
        return err;
    }
    
    err = data_service_init();
    if (err) {
        printk("BLE Services: Failed to initialize Data Service (err %d)\n", err);
        return err;
    }
    */
    
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
    printk("BLE Services: Note - Custom services temporarily disabled due to Zephyr macro conflicts\n");
    
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
    } else {
        if (active_connections > 0) {
            active_connections--;
        }
    }
    
    printk("BLE Services: Connection event - %s (active: %d)\n", 
           connected ? "connected" : "disconnected", active_connections);
    
    /* Notify all services of connection events */
    /* Temporarily disabled due to Zephyr BLE macro conflicts
    dfu_service_connection_event(conn, connected);
    control_service_connection_event(conn, connected);
    data_service_connection_event(conn, connected);
    wasm_service_connection_event(conn, connected);
    */
}

uint8_t ble_services_get_device_status(void)
{
    if (!services_initialized) {
        return 0; // Not initialized
    }
    
    /* Temporarily return a fixed status since control service is disabled */
    return 1; // DEVICE_STATUS_IDLE equivalent
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
