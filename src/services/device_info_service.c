#include "device_info_service.h"
#include "ble_packet_handlers.h"
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
 * SIMPLIFIED CHARACTERISTIC HANDLERS
 * ============================================================================ */

// The macro will generate read_manufacturer_name() wrapper that calls this
static ssize_t simple_read_manufacturer_name(device_info_string_t *response)
{
    strncpy(response->text, DEVICE_MANUFACTURER_NAME, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    return strlen(response->text);
}

// The macro will generate read_model_number() wrapper that calls this
static ssize_t simple_read_model_number(device_info_string_t *response)
{
    strncpy(response->text, DEVICE_MODEL_NUMBER, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    return strlen(response->text);
}

// The macro will generate read_firmware_revision() wrapper that calls this
static ssize_t simple_read_firmware_revision(device_info_string_t *response)
{
    strncpy(response->text, firmware_revision, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    return strlen(response->text);
}

// The macro will generate read_hardware_revision() wrapper that calls this
static ssize_t simple_read_hardware_revision(device_info_string_t *response)
{
    strncpy(response->text, DEVICE_HARDWARE_REVISION, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    return strlen(response->text);
}

// The macro will generate read_software_revision() wrapper that calls this
static ssize_t simple_read_software_revision(device_info_string_t *response)
{
    strncpy(response->text, software_revision, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    return strlen(response->text);
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(device_info_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),
    BT_GATT_CHARACTERISTIC_SIMPLE(BT_UUID_DIS_MANUFACTURER_NAME,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_manufacturer_name, NULL, NULL,
                          device_info_string_t, void),
    BT_GATT_CHARACTERISTIC_SIMPLE(BT_UUID_DIS_MODEL_NUMBER,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_model_number, NULL, NULL,
                          device_info_string_t, void),
    BT_GATT_CHARACTERISTIC_SIMPLE(BT_UUID_DIS_FIRMWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_firmware_revision, NULL, NULL,
                          device_info_string_t, void),
    BT_GATT_CHARACTERISTIC_SIMPLE(BT_UUID_DIS_HARDWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_hardware_revision, NULL, NULL,
                          device_info_string_t, void),
    BT_GATT_CHARACTERISTIC_SIMPLE(BT_UUID_DIS_SOFTWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          read_software_revision, NULL, NULL,
                          device_info_string_t, void),
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
