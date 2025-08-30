#ifndef DEVICE_INFO_SERVICE_H
#define DEVICE_INFO_SERVICE_H

#include <zephyr/bluetooth/gatt.h>
#include <stdint.h>

/**
 * @file device_info_service.h
 * @brief Device Information Service (0x180A) implementation
 * 
 * Standard Bluetooth SIG service providing device identification information.
 */

/* ============================================================================
 * PACKET TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Device information string packet structure
 * 
 * Used for all device information string responses.
 * Total size: 64 bytes
 */
typedef struct {
    char text[64];  ///< Null-terminated string (up to 63 chars + null)
} __attribute__((packed)) device_info_string_t;

/* ============================================================================
 * DEVICE INFORMATION CONSTANTS
 * ============================================================================ */

#define DEVICE_MANUFACTURER_NAME    "Nordic Semiconductor"
#define DEVICE_MODEL_NUMBER         "nRF5340-DK"
#define DEVICE_FIRMWARE_REVISION    "v1.0.0"
#define DEVICE_HARDWARE_REVISION    "PCA10095"
#define DEVICE_SOFTWARE_REVISION    "Zephyr 3.5.0"

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize Device Information Service
 * 
 * Registers the standard Device Information Service (0x180A) with
 * manufacturer name, model number, and revision information.
 * 
 * @return 0 on success, negative error code on failure
 */
int device_info_service_init(void);

/**
 * @brief Update firmware revision string
 * @param revision New firmware revision string
 * @return 0 on success, negative error code on failure
 */
int device_info_update_firmware_revision(const char *revision);

/**
 * @brief Update software revision string  
 * @param revision New software revision string
 * @return 0 on success, negative error code on failure
 */
int device_info_update_software_revision(const char *revision);

#endif /* DEVICE_INFO_SERVICE_H */
