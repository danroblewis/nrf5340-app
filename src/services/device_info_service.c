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
static ssize_t manufacturer_name_handler(device_info_string_t *response)
{
    printk("\n=== Device Info Service: manufacturer_name_handler called ===\n");
    printk("Device Info: Reading manufacturer name\n");
    strncpy(response->text, DEVICE_MANUFACTURER_NAME, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    printk("Device Info: Returning manufacturer: %s\n", response->text);
    return strlen(response->text);
}

// The macro will generate read_model_number() wrapper that calls this
static ssize_t model_number_handler(device_info_string_t *response)
{
    printk("\n=== Device Info Service: model_number_handler called ===\n");
    printk("Device Info: Reading model number\n");
    strncpy(response->text, DEVICE_MODEL_NUMBER, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    printk("Device Info: Returning model: %s\n", response->text);
    return strlen(response->text);
}

// The macro will generate read_firmware_revision() wrapper that calls this
static ssize_t firmware_revision_handler(device_info_string_t *response)
{
    printk("\n=== Device Info Service: firmware_revision_handler called ===\n");
    printk("Device Info: Reading firmware revision\n");
    strncpy(response->text, firmware_revision, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    printk("Device Info: Returning firmware: %s\n", response->text);
    return strlen(response->text);
}

// The macro will generate read_hardware_revision() wrapper that calls this
static ssize_t hardware_revision_handler(device_info_string_t *response)
{
    printk("\n=== Device Info Service: hardware_revision_handler called ===\n");
    printk("Device Info: Reading hardware revision\n");
    strncpy(response->text, DEVICE_HARDWARE_REVISION, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    printk("Device Info: Returning hardware: %s\n", response->text);
    return strlen(response->text);
}

// The macro will generate read_software_revision() wrapper that calls this
static ssize_t software_revision_handler(device_info_string_t *response)
{
    printk("\n=== Device Info Service: software_revision_handler called ===\n");
    printk("Device Info: Reading software revision\n");
    strncpy(response->text, software_revision, sizeof(response->text) - 1);
    response->text[sizeof(response->text) - 1] = '\0';
    printk("Device Info: Returning software: %s\n", response->text);
    return strlen(response->text);
}

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

/* ============================================================================
 * CLEAN HANDLERS - WORK WITH STRUCTS DIRECTLY
 * ============================================================================ */

/* Declare the clean handlers we want to write */
DECLARE_READ_HANDLER(manufacturer_name_handler, device_info_string_t);
DECLARE_READ_HANDLER(model_number_handler, device_info_string_t);
DECLARE_READ_HANDLER(firmware_revision_handler, device_info_string_t);
DECLARE_READ_HANDLER(hardware_revision_handler, device_info_string_t);
DECLARE_READ_HANDLER(software_revision_handler, device_info_string_t);

/* Generate BLE wrappers automatically */
BLE_READ_WRAPPER(manufacturer_name_handler, device_info_string_t)
BLE_READ_WRAPPER(model_number_handler, device_info_string_t)
BLE_READ_WRAPPER(firmware_revision_handler, device_info_string_t)
BLE_READ_WRAPPER(hardware_revision_handler, device_info_string_t)
BLE_READ_WRAPPER(software_revision_handler, device_info_string_t)

BT_GATT_SERVICE_DEFINE(device_info_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          manufacturer_name_handler_ble, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MODEL_NUMBER,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          model_number_handler_ble, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_FIRMWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          firmware_revision_handler_ble, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_HARDWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          hardware_revision_handler_ble, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DIS_SOFTWARE_REVISION,
                          BT_GATT_CHRC_READ,
                          BT_GATT_PERM_READ,
                          software_revision_handler_ble, NULL, NULL),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int device_info_service_init(void)
{
    printk("Device Info Service: 🔧 Initializing Device Information Service...\n");
    printk("Device Info Service: Registering 5 characteristics:\n");
    printk("  📝 Manufacturer: %s\n", DEVICE_MANUFACTURER_NAME);
    printk("  📝 Model: %s\n", DEVICE_MODEL_NUMBER);
    printk("  📝 Firmware: %s\n", firmware_revision);
    printk("  📝 Hardware: %s\n", DEVICE_HARDWARE_REVISION);
    printk("  📝 Software: %s\n", software_revision);
    printk("Device Info Service: ✅ Service ready for BLE clients\n");
    
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
