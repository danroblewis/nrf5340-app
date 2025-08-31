#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/* Include our modular BLE services */
#include "services/ble_services.h"

/**
 * @file main.c
 * @brief Main application for nRF5340 Multi-Service BLE Device
 * 
 * This application demonstrates industry-standard BLE service architecture
 * with proper separation of concerns across multiple service modules.
 */

/* ============================================================================
 * BLE CONNECTION MANAGEMENT
 * ============================================================================ */

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }
    
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    
    printk("Connected to %s\n", addr);
    printk("Connection handle: %d\n", bt_conn_index(conn));
    
    /* Notify all services of connection */
    ble_services_connection_event(conn, true);
    
    /* Request MTU exchange for large packet support */
    int mtu_err = ble_services_request_mtu_exchange(conn);
    if (mtu_err) {
        printk("MTU exchange request failed (err %d)\n", mtu_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    
    printk("Disconnected from %s (reason %u)\n", addr, reason);
    
    /* Notify all services of disconnection */
    ble_services_connection_event(conn, false);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/* ============================================================================
 * BLE INITIALIZATION
 * ============================================================================ */

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");
    
    /* Initialize all BLE services */
    err = ble_services_init();
    if (err) {
        printk("Failed to initialize BLE services (err %d)\n", err);
        return;
    }

    /* Start advertising */
    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, NULL, 0, NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising successfully started\n");
    printk("Device name: nRF5340-BLE-Multi-Service\n");
    printk("Ready for connections...\n");
}

/* ============================================================================
 * APPLICATION STATUS MONITORING
 * ============================================================================ */

static void print_status_summary(void)
{
    uint8_t device_status = ble_services_get_device_status();
    const char *status_str;
    
    switch (device_status) {
    case 0:
        status_str = "idle";
        break;
    case 1:
        status_str = "connected";
        break;
    case 2:
        status_str = "error";
        break;
    default:
        status_str = "unknown";
        break;
    }
    
    printk("Status: Device=%s, Uptime=%lld seconds\n",
           status_str, k_uptime_get() / 1000);
}

/* ============================================================================
 * MAIN APPLICATION
 * ============================================================================ */

int main(void)
{
    int err;

    /* Print startup banner */
    printk("\n");
    printk("========================================\n");
    printk("nRF5340 Multi-Service BLE Device\n");
    printk("Build: %s %s\n", __DATE__, __TIME__);
    printk("========================================\n");
    printk("Industry-Standard BLE Implementation\n");
    printk("Modular Service Architecture\n");
    printk("========================================\n");

    /* Initialize the Bluetooth subsystem */
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    /* Register connection callbacks */
    bt_conn_cb_register(&conn_callbacks);

    printk("BLE device initialization complete\n");
    printk("Waiting for connections...\n");

    /* Main application loop */
    while (1) {
        /* Periodic status updates every 30 seconds */
        k_sleep(K_SECONDS(30));
        print_status_summary();
    }

    return 0;
}