#include "device_info_service.h"
#include <zephyr/sys/printk.h>
#include <string.h>

/**
 * @file device_info_service.c
 * @brief Device Information Service (0x180A) implementation
 */

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

static char firmware_revision[32] = DEVICE_FIRMWARE_REVISION;
static char software_revision[32] = DEVICE_SOFTWARE_REVISION;

/* ============================================================================
 * CHARACTERISTIC READ HANDLERS
 * ============================================================================ */

static ssize_t read_manufacturer_name(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           DEVICE_MANUFACTURER_NAME, strlen(DEVICE_MANUFACTURER_NAME));
}

static ssize_t read_model_number(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           DEVICE_MODEL_NUMBER, strlen(DEVICE_MODEL_NUMBER));
}

static ssize_t read_firmware_revision(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           firmware_revision, strlen(firmware_revision));
}

static ssize_t read_hardware_revision(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           DEVICE_HARDWARE_REVISION, strlen(DEVICE_HARDWARE_REVISION));
}

static ssize_t read_software_revision(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           software_revision, strlen(software_revision));
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(device_info_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_manufacturer_name, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MODEL_NUMBER,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_model_number, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_FIRMWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_firmware_revision, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_HARDWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_hardware_revision, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_SOFTWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_software_revision, NULL, NULL),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int device_info_service_init(void)
{
    printk("Device Info Service: Initialized\n");
    printk("  Manufacturer: %s\n", DEVICE_MANUFACTURER_NAME);
    printk("  Model: %s\n", DEVICE_MODEL_NUMBER);
    printk("  Firmware: %s\n", firmware_revision);
    printk("  Hardware: %s\n", DEVICE_HARDWARE_REVISION);
    printk("  Software: %s\n", software_revision);
    
    return 0;
}

int device_info_update_firmware_revision(const char *revision)
{
    if (!revision || strlen(revision) >= sizeof(firmware_revision)) {
        return -EINVAL;
    }
    
    strncpy(firmware_revision, revision, sizeof(firmware_revision) - 1);
    firmware_revision[sizeof(firmware_revision) - 1] = '\0';
    
    printk("Device Info Service: Firmware revision updated to %s\n", firmware_revision);
    return 0;
}

int device_info_update_software_revision(const char *revision)
{
    if (!revision || strlen(revision) >= sizeof(software_revision)) {
        return -EINVAL;
    }
    
    strncpy(software_revision, revision, sizeof(software_revision) - 1);
    software_revision[sizeof(software_revision) - 1] = '\0';
    
    printk("Device Info Service: Software revision updated to %s\n", software_revision);
    return 0;
}
