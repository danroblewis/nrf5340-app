#ifndef SPRITE_SERVICE_H
#define SPRITE_SERVICE_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <stdint.h>

/**
 * @file sprite_service.h
 * @brief Sprite Registry Service implementation
 * 
 * Manages a registry of 16x16 monochrome bitmap sprites with CRC verification.
 * Supports upload, download, and verification of sprite data over BLE.
 * 
 * Features:
 * - 16x16 monochrome bitmaps (32 bytes each)
 * - Sprite ID management (0-65535)
 * - CRC16 data integrity verification
 * - Efficient storage and retrieval
 * - Large packet support via MTU negotiation
 */

/* ============================================================================
 * SPRITE SPECIFICATIONS
 * ============================================================================ */

#define SPRITE_WIDTH                16      /* Sprite width in pixels */
#define SPRITE_HEIGHT               16      /* Sprite height in pixels */
#define SPRITE_PIXELS               (SPRITE_WIDTH * SPRITE_HEIGHT)  /* 256 pixels */
#define SPRITE_DATA_SIZE            (SPRITE_PIXELS / 8)             /* 32 bytes (1 bit per pixel) */
#define SPRITE_MAX_COUNT            256     /* Maximum sprites in registry */
#define SPRITE_ID_INVALID           0xFFFF  /* Invalid sprite ID marker */

/* ============================================================================
 * PACKET TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Sprite upload packet structure
 * 
 * Used for uploading sprite data with ID and CRC verification.
 * Total size: 36 bytes (fits comfortably in 244-byte MTU)
 * 
 * Packet format:
 * - sprite_id: 2 bytes (0-65535)
 * - bitmap_data: 32 bytes (16x16 monochrome bitmap)
 * - crc16: 2 bytes (CRC16 checksum of bitmap_data)
 */
typedef struct {
    uint16_t sprite_id;                     ///< Sprite ID (0-65535)
    uint8_t bitmap_data[SPRITE_DATA_SIZE];  ///< 16x16 monochrome bitmap (32 bytes)
    uint16_t crc16;                         ///< CRC16 checksum of bitmap_data
} __attribute__((packed)) sprite_upload_packet_t;

/**
 * @brief Sprite download request packet structure
 * 
 * Used for requesting a specific sprite by ID.
 * Total size: 2 bytes
 */
typedef struct {
    uint16_t sprite_id;                     ///< Requested sprite ID
} __attribute__((packed)) sprite_download_request_t;

/**
 * @brief Sprite download response packet structure
 * 
 * Used for returning sprite data with verification info.
 * Total size: 37 bytes
 */
typedef struct {
    uint16_t sprite_id;                     ///< Sprite ID
    uint8_t bitmap_data[SPRITE_DATA_SIZE];  ///< 16x16 monochrome bitmap (32 bytes)
    uint16_t crc16;                         ///< CRC16 checksum of bitmap_data
    uint8_t status;                         ///< Status (SPRITE_STATUS_*)
} __attribute__((packed)) sprite_download_packet_t;

/**
 * @brief Sprite registry status packet structure
 * 
 * Used for reporting registry status and statistics.
 * Total size: 12 bytes
 */
typedef struct {
    uint16_t total_sprites;                 ///< Total sprites in registry
    uint16_t free_slots;                    ///< Available sprite slots
    uint16_t last_sprite_id;                ///< Last uploaded sprite ID
    uint8_t registry_status;                ///< Registry status (REGISTRY_STATUS_*)
    uint8_t last_operation;                 ///< Last operation performed
    uint16_t crc_errors;                    ///< Total CRC errors encountered
    uint16_t reserved;                      ///< Reserved for future use
} __attribute__((packed)) sprite_registry_status_t;

/**
 * @brief Sprite verification request packet structure
 * 
 * Used for verifying a sprite's CRC without downloading full data.
 * Total size: 2 bytes
 */
typedef struct {
    uint16_t sprite_id;                     ///< Sprite ID to verify
} __attribute__((packed)) sprite_verify_request_t;

/**
 * @brief Sprite verification response packet structure
 * 
 * Used for returning verification results.
 * Total size: 8 bytes
 */
typedef struct {
    uint16_t sprite_id;                     ///< Verified sprite ID
    uint16_t stored_crc16;                  ///< CRC16 stored with sprite
    uint16_t calculated_crc16;              ///< CRC16 calculated from current data
    uint8_t verification_status;            ///< Verification result (VERIFY_STATUS_*)
    uint8_t reserved;                       ///< Reserved for future use
} __attribute__((packed)) sprite_verify_response_t;

/* ============================================================================
 * SPRITE SERVICE DEFINITIONS
 * ============================================================================ */

/* Static UUID definitions to avoid macro conflicts */
static const struct bt_uuid_16 sprite_service_uuid = BT_UUID_INIT_16(0xFFF8);
static const struct bt_uuid_16 sprite_upload_uuid = BT_UUID_INIT_16(0xFFF9);
static const struct bt_uuid_16 sprite_download_request_uuid = BT_UUID_INIT_16(0xFFFA);
static const struct bt_uuid_16 sprite_download_response_uuid = BT_UUID_INIT_16(0xFFFB);
static const struct bt_uuid_16 sprite_registry_status_uuid = BT_UUID_INIT_16(0xFFFC);
static const struct bt_uuid_16 sprite_verify_request_uuid = BT_UUID_INIT_16(0xFFFD);
static const struct bt_uuid_16 sprite_verify_response_uuid = BT_UUID_INIT_16(0xFFFE);

#define SPRITE_SERVICE_UUID             (&sprite_service_uuid.uuid)
#define SPRITE_UPLOAD_UUID              (&sprite_upload_uuid.uuid)
#define SPRITE_DOWNLOAD_REQUEST_UUID    (&sprite_download_request_uuid.uuid)
#define SPRITE_DOWNLOAD_RESPONSE_UUID   (&sprite_download_response_uuid.uuid)
#define SPRITE_REGISTRY_STATUS_UUID     (&sprite_registry_status_uuid.uuid)
#define SPRITE_VERIFY_REQUEST_UUID      (&sprite_verify_request_uuid.uuid)
#define SPRITE_VERIFY_RESPONSE_UUID     (&sprite_verify_response_uuid.uuid)

/* ============================================================================
 * STATUS CODES AND CONSTANTS
 * ============================================================================ */

/* Sprite status codes */
#define SPRITE_STATUS_SUCCESS           0x00    /* Operation successful */
#define SPRITE_STATUS_NOT_FOUND         0x01    /* Sprite ID not found */
#define SPRITE_STATUS_CRC_ERROR         0x02    /* CRC verification failed */
#define SPRITE_STATUS_REGISTRY_FULL     0x03    /* Registry is full */
#define SPRITE_STATUS_INVALID_ID        0x04    /* Invalid sprite ID */
#define SPRITE_STATUS_INVALID_DATA      0x05    /* Invalid bitmap data */

/* Registry status codes */
#define REGISTRY_STATUS_READY           0x00    /* Registry ready for operations */
#define REGISTRY_STATUS_BUSY            0x01    /* Registry busy processing */
#define REGISTRY_STATUS_ERROR           0x02    /* Registry error state */
#define REGISTRY_STATUS_FULL            0x03    /* Registry full */

/* Operation codes */
#define OPERATION_NONE                  0x00    /* No operation */
#define OPERATION_UPLOAD                0x01    /* Sprite upload */
#define OPERATION_DOWNLOAD              0x02    /* Sprite download */
#define OPERATION_VERIFY                0x03    /* Sprite verification */
#define OPERATION_STATUS                0x04    /* Status query */

/* Verification status codes */
#define VERIFY_STATUS_VALID             0x00    /* CRC verification passed */
#define VERIFY_STATUS_INVALID           0x01    /* CRC verification failed */
#define VERIFY_STATUS_NOT_FOUND         0x02    /* Sprite not found */
#define VERIFY_STATUS_ERROR             0x03    /* Verification error */

/* ============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * @brief Initialize the sprite registry service
 * @return 0 on success, negative error code on failure
 */
int sprite_service_init(void);

/**
 * @brief Handle BLE connection events for sprite service
 * @param conn Connection handle
 * @param connected True if connected, false if disconnected
 */
void sprite_service_connection_event(struct bt_conn *conn, bool connected);

/**
 * @brief Get sprite registry status
 * @return Current registry status
 */
uint8_t sprite_service_get_registry_status(void);

/**
 * @brief Get number of sprites in registry
 * @return Number of stored sprites
 */
uint16_t sprite_service_get_sprite_count(void);

/**
 * @brief Check if sprite ID exists in registry
 * @param sprite_id Sprite ID to check
 * @return True if sprite exists, false otherwise
 */
bool sprite_service_sprite_exists(uint16_t sprite_id);

/**
 * @brief Calculate CRC16 for sprite bitmap data
 * @param data Bitmap data
 * @param length Data length
 * @return CRC16 checksum
 */
uint16_t sprite_service_calculate_crc16(const uint8_t *data, uint16_t length);

/**
 * @brief Clear all sprites from registry
 * @return 0 on success, negative error code on failure
 */
int sprite_service_clear_registry(void);

/**
 * @brief Get registry statistics
 * @param total_sprites Pointer to store total sprite count
 * @param free_slots Pointer to store free slot count
 * @param crc_errors Pointer to store CRC error count
 */
void sprite_service_get_statistics(uint16_t *total_sprites, uint16_t *free_slots, uint16_t *crc_errors);

/* ============================================================================
 * UTILITY MACROS
 * ============================================================================ */

/**
 * @brief Convert pixel coordinates to bit position in bitmap
 * @param x X coordinate (0-15)
 * @param y Y coordinate (0-15)
 * @return Bit position in bitmap array
 */
#define SPRITE_PIXEL_TO_BIT(x, y) ((y) * SPRITE_WIDTH + (x))

/**
 * @brief Convert bit position to byte index and bit offset
 * @param bit_pos Bit position
 * @param byte_idx Pointer to store byte index
 * @param bit_offset Pointer to store bit offset
 */
#define SPRITE_BIT_TO_BYTE_OFFSET(bit_pos, byte_idx, bit_offset) \
    do { \
        *(byte_idx) = (bit_pos) / 8; \
        *(bit_offset) = (bit_pos) % 8; \
    } while(0)

/**
 * @brief Get pixel value from bitmap
 * @param bitmap Bitmap data array
 * @param x X coordinate (0-15)
 * @param y Y coordinate (0-15)
 * @return Pixel value (0 or 1)
 */
#define SPRITE_GET_PIXEL(bitmap, x, y) \
    (((bitmap)[SPRITE_PIXEL_TO_BIT(x, y) / 8] >> (SPRITE_PIXEL_TO_BIT(x, y) % 8)) & 1)

/**
 * @brief Set pixel value in bitmap
 * @param bitmap Bitmap data array
 * @param x X coordinate (0-15)
 * @param y Y coordinate (0-15)
 * @param value Pixel value (0 or 1)
 */
#define SPRITE_SET_PIXEL(bitmap, x, y, value) \
    do { \
        uint8_t byte_idx = SPRITE_PIXEL_TO_BIT(x, y) / 8; \
        uint8_t bit_offset = SPRITE_PIXEL_TO_BIT(x, y) % 8; \
        if (value) { \
            (bitmap)[byte_idx] |= (1 << bit_offset); \
        } else { \
            (bitmap)[byte_idx] &= ~(1 << bit_offset); \
        } \
    } while(0)

#endif /* SPRITE_SERVICE_H */
