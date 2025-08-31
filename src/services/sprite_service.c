#include "sprite_service.h"
#include "ble_packet_handlers.h"
#include "ble_services.h"
#include <zephyr/sys/printk.h>
#include <string.h>

/**
 * @file sprite_service.c
 * @brief Sprite Registry Service implementation
 * 
 * Manages a registry of 16x16 monochrome bitmap sprites with CRC verification.
 */

/* ============================================================================
 * STATIC DATA AND STORAGE
 * ============================================================================ */

/* Sprite storage structure */
typedef struct {
    uint16_t sprite_id;                     /* Sprite ID */
    uint8_t bitmap_data[SPRITE_DATA_SIZE];  /* Bitmap data */
    uint16_t crc16;                         /* CRC16 checksum */
    bool is_valid;                          /* Slot validity flag */
} sprite_slot_t;

/* Registry storage */
static sprite_slot_t sprite_registry[SPRITE_MAX_COUNT];
static uint16_t sprite_count = 0;
static uint16_t crc_error_count = 0;
static uint8_t registry_status = REGISTRY_STATUS_READY;
static uint8_t last_operation = OPERATION_NONE;
static uint16_t last_sprite_id = SPRITE_ID_INVALID;
static struct bt_conn *sprite_conn = NULL;

/* ============================================================================
 * CRC16 IMPLEMENTATION
 * ============================================================================ */

/**
 * @brief Calculate CRC16 using polynomial 0x1021 (CCITT)
 */
uint16_t sprite_service_calculate_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;  /* Initial value */
    const uint16_t polynomial = 0x1021;  /* CCITT polynomial */
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/* ============================================================================
 * SPRITE REGISTRY MANAGEMENT
 * ============================================================================ */

/**
 * @brief Find sprite slot by ID
 * @param sprite_id Sprite ID to find
 * @return Pointer to sprite slot, or NULL if not found
 */
static sprite_slot_t* find_sprite_slot(uint16_t sprite_id)
{
    for (uint16_t i = 0; i < SPRITE_MAX_COUNT; i++) {
        if (sprite_registry[i].is_valid && sprite_registry[i].sprite_id == sprite_id) {
            return &sprite_registry[i];
        }
    }
    return NULL;
}

/**
 * @brief Find free sprite slot
 * @return Pointer to free sprite slot, or NULL if registry is full
 */
static sprite_slot_t* find_free_slot(void)
{
    for (uint16_t i = 0; i < SPRITE_MAX_COUNT; i++) {
        if (!sprite_registry[i].is_valid) {
            return &sprite_registry[i];
        }
    }
    return NULL;
}

/**
 * @brief Store sprite in registry
 * @param sprite_id Sprite ID
 * @param bitmap_data Bitmap data
 * @param crc16 CRC16 checksum
 * @return SPRITE_STATUS_* code
 */
static uint8_t store_sprite(uint16_t sprite_id, const uint8_t *bitmap_data, uint16_t crc16)
{
    /* Verify CRC */
    uint16_t calculated_crc = sprite_service_calculate_crc16(bitmap_data, SPRITE_DATA_SIZE);
    if (calculated_crc != crc16) {
        printk("Sprite Service: CRC mismatch for ID %d (got 0x%04x, expected 0x%04x)\n", 
               sprite_id, calculated_crc, crc16);
        crc_error_count++;
        return SPRITE_STATUS_CRC_ERROR;
    }
    
    /* Find existing sprite or free slot */
    sprite_slot_t *slot = find_sprite_slot(sprite_id);
    bool is_update = (slot != NULL);
    
    if (!slot) {
        slot = find_free_slot();
        if (!slot) {
            printk("Sprite Service: Registry full, cannot store sprite %d\n", sprite_id);
            return SPRITE_STATUS_REGISTRY_FULL;
        }
    }
    
    /* Store sprite data */
    slot->sprite_id = sprite_id;
    memcpy(slot->bitmap_data, bitmap_data, SPRITE_DATA_SIZE);
    slot->crc16 = crc16;
    slot->is_valid = true;
    
    if (!is_update) {
        sprite_count++;
    }
    
    last_sprite_id = sprite_id;
    
    printk("Sprite Service: %s sprite %d (CRC: 0x%04x)\n", 
           is_update ? "Updated" : "Stored", sprite_id, crc16);
    
    return SPRITE_STATUS_SUCCESS;
}

/* ============================================================================
 * BLE CHARACTERISTIC HANDLERS
 * ============================================================================ */

/**
 * @brief Handle sprite upload requests
 */
static ssize_t sprite_upload_handler(const sprite_upload_packet_t *packet)
{
    printk("\n=== Sprite Service: sprite_upload_handler called ===\n");
    printk("Sprite Service: Upload request for sprite %d\n", packet->sprite_id);
    
    registry_status = REGISTRY_STATUS_BUSY;
    last_operation = OPERATION_UPLOAD;
    
    /* Validate sprite ID */
    if (packet->sprite_id == SPRITE_ID_INVALID) {
        printk("Sprite Service: Invalid sprite ID\n");
        registry_status = REGISTRY_STATUS_ERROR;
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    /* Store sprite */
    uint8_t status = store_sprite(packet->sprite_id, packet->bitmap_data, packet->crc16);
    
    if (status == SPRITE_STATUS_SUCCESS) {
        registry_status = REGISTRY_STATUS_READY;
        printk("Sprite Service: Successfully stored sprite %d\n", packet->sprite_id);
        return sizeof(*packet);
    } else {
        registry_status = REGISTRY_STATUS_ERROR;
        printk("Sprite Service: Failed to store sprite %d (status: %d)\n", packet->sprite_id, status);
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }
}

/**
 * @brief Handle sprite download requests
 */
static ssize_t sprite_download_request_handler(const sprite_download_request_t *packet)
{
    printk("\n=== Sprite Service: sprite_download_request_handler called ===\n");
    printk("Sprite Service: Download request for sprite %d\n", packet->sprite_id);
    
    last_operation = OPERATION_DOWNLOAD;
    last_sprite_id = packet->sprite_id;
    
    /* This is a write-only characteristic that triggers a download response */
    /* The actual response is sent via the download response characteristic */
    
    return sizeof(*packet);
}

/**
 * @brief Handle sprite download response reads
 */
static ssize_t sprite_download_response_handler(sprite_download_packet_t *response)
{
    printk("\n=== Sprite Service: sprite_download_response_handler called ===\n");
    printk("Sprite Service: Preparing download response for sprite %d\n", last_sprite_id);
    
    /* Find sprite */
    sprite_slot_t *slot = find_sprite_slot(last_sprite_id);
    
    response->sprite_id = last_sprite_id;
    
    if (slot) {
        /* Copy sprite data */
        memcpy(response->bitmap_data, slot->bitmap_data, SPRITE_DATA_SIZE);
        response->crc16 = slot->crc16;
        response->status = SPRITE_STATUS_SUCCESS;
        
        printk("Sprite Service: Returning sprite %d (CRC: 0x%04x)\n", 
               last_sprite_id, slot->crc16);
    } else {
        /* Sprite not found */
        memset(response->bitmap_data, 0, SPRITE_DATA_SIZE);
        response->crc16 = 0;
        response->status = SPRITE_STATUS_NOT_FOUND;
        
        printk("Sprite Service: Sprite %d not found\n", last_sprite_id);
    }
    
    return sizeof(*response);
}

/**
 * @brief Handle registry status requests
 */
static ssize_t sprite_registry_status_handler(sprite_registry_status_t *response)
{
    printk("\n=== Sprite Service: sprite_registry_status_handler called ===\n");
    printk("Sprite Service: Status request\n");
    
    response->total_sprites = sprite_count;
    response->free_slots = SPRITE_MAX_COUNT - sprite_count;
    response->last_sprite_id = last_sprite_id;
    response->registry_status = registry_status;
    response->last_operation = last_operation;
    response->crc_errors = crc_error_count;
    response->reserved = 0;
    
    printk("Sprite Service: Status - %d sprites, %d free slots, %d CRC errors\n",
           sprite_count, SPRITE_MAX_COUNT - sprite_count, crc_error_count);
    
    return sizeof(*response);
}

/**
 * @brief Handle sprite verification requests
 */
static ssize_t sprite_verify_request_handler(const sprite_verify_request_t *packet)
{
    printk("\n=== Sprite Service: sprite_verify_request_handler called ===\n");
    printk("Sprite Service: Verification request for sprite %d\n", packet->sprite_id);
    
    last_operation = OPERATION_VERIFY;
    last_sprite_id = packet->sprite_id;
    
    return sizeof(*packet);
}

/**
 * @brief Handle sprite verification response reads
 */
static ssize_t sprite_verify_response_handler(sprite_verify_response_t *response)
{
    printk("\n=== Sprite Service: sprite_verify_response_handler called ===\n");
    printk("Sprite Service: Preparing verification response for sprite %d\n", last_sprite_id);
    
    sprite_slot_t *slot = find_sprite_slot(last_sprite_id);
    
    response->sprite_id = last_sprite_id;
    response->reserved = 0;
    
    if (slot) {
        /* Calculate current CRC */
        uint16_t calculated_crc = sprite_service_calculate_crc16(slot->bitmap_data, SPRITE_DATA_SIZE);
        
        response->stored_crc16 = slot->crc16;
        response->calculated_crc16 = calculated_crc;
        
        if (calculated_crc == slot->crc16) {
            response->verification_status = VERIFY_STATUS_VALID;
            printk("Sprite Service: Sprite %d verification PASSED\n", last_sprite_id);
        } else {
            response->verification_status = VERIFY_STATUS_INVALID;
            printk("Sprite Service: Sprite %d verification FAILED (stored: 0x%04x, calculated: 0x%04x)\n",
                   last_sprite_id, slot->crc16, calculated_crc);
        }
    } else {
        response->stored_crc16 = 0;
        response->calculated_crc16 = 0;
        response->verification_status = VERIFY_STATUS_NOT_FOUND;
        printk("Sprite Service: Sprite %d not found for verification\n", last_sprite_id);
    }
    
    return sizeof(*response);
}

/* ============================================================================
 * BLE WRAPPER GENERATION
 * ============================================================================ */

/* Generate BLE wrappers automatically */
BLE_WRITE_WRAPPER(sprite_upload_handler, sprite_upload_packet_t)
BLE_WRITE_WRAPPER(sprite_download_request_handler, sprite_download_request_t)
BLE_READ_WRAPPER(sprite_download_response_handler, sprite_download_packet_t)
BLE_READ_WRAPPER(sprite_registry_status_handler, sprite_registry_status_t)
BLE_WRITE_WRAPPER(sprite_verify_request_handler, sprite_verify_request_t)
BLE_READ_WRAPPER(sprite_verify_response_handler, sprite_verify_response_t)

/* ============================================================================
 * SERVICE DEFINITION
 * ============================================================================ */

BT_GATT_SERVICE_DEFINE(sprite_service,
    BT_GATT_PRIMARY_SERVICE(SPRITE_SERVICE_UUID),
    
    /* Sprite Upload - Write sprite data with CRC */
    BT_GATT_CHARACTERISTIC(SPRITE_UPLOAD_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, sprite_upload_handler_ble, NULL),
    
    /* Sprite Download Request - Write sprite ID to request */
    BT_GATT_CHARACTERISTIC(SPRITE_DOWNLOAD_REQUEST_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, sprite_download_request_handler_ble, NULL),
    
    /* Sprite Download Response - Read sprite data */
    BT_GATT_CHARACTERISTIC(SPRITE_DOWNLOAD_RESPONSE_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          sprite_download_response_handler_ble, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    
    /* Registry Status - Read registry statistics */
    BT_GATT_CHARACTERISTIC(SPRITE_REGISTRY_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          sprite_registry_status_handler_ble, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    
    /* Sprite Verification Request - Write sprite ID to verify */
    BT_GATT_CHARACTERISTIC(SPRITE_VERIFY_REQUEST_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, sprite_verify_request_handler_ble, NULL),
    
    /* Sprite Verification Response - Read verification results */
    BT_GATT_CHARACTERISTIC(SPRITE_VERIFY_RESPONSE_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          sprite_verify_response_handler_ble, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

int sprite_service_init(void)
{
    /* Initialize registry */
    memset(sprite_registry, 0, sizeof(sprite_registry));
    sprite_count = 0;
    crc_error_count = 0;
    registry_status = REGISTRY_STATUS_READY;
    last_operation = OPERATION_NONE;
    last_sprite_id = SPRITE_ID_INVALID;
    sprite_conn = NULL;
    
    printk("Sprite Service: Initialized\n");
    printk("  Service UUID: 0xFFF8\n");
    printk("  Max sprites: %d\n", SPRITE_MAX_COUNT);
    printk("  Sprite size: %dx%d pixels (%d bytes)\n", 
           SPRITE_WIDTH, SPRITE_HEIGHT, SPRITE_DATA_SIZE);
    printk("  Upload packet size: %zu bytes\n", sizeof(sprite_upload_packet_t));
    printk("  Download packet size: %zu bytes\n", sizeof(sprite_download_packet_t));
    printk("  Characteristics:\n");
    printk("    - Upload (0xFFF9): WRITE\n");
    printk("    - Download Request (0xFFFA): WRITE\n");
    printk("    - Download Response (0xFFFB): READ + NOTIFY\n");
    printk("    - Registry Status (0xFFFC): READ + NOTIFY\n");
    printk("    - Verify Request (0xFFFD): WRITE\n");
    printk("    - Verify Response (0xFFFE): READ + NOTIFY\n");
    
    return 0;
}

void sprite_service_connection_event(struct bt_conn *conn, bool connected)
{
    if (connected) {
        printk("Sprite Service: Client connected\n");
        sprite_conn = conn;
    } else {
        printk("Sprite Service: Client disconnected\n");
        if (conn == sprite_conn) {
            sprite_conn = NULL;
        }
    }
}

uint8_t sprite_service_get_registry_status(void)
{
    return registry_status;
}

uint16_t sprite_service_get_sprite_count(void)
{
    return sprite_count;
}

bool sprite_service_sprite_exists(uint16_t sprite_id)
{
    return find_sprite_slot(sprite_id) != NULL;
}

int sprite_service_clear_registry(void)
{
    printk("Sprite Service: Clearing registry\n");
    
    memset(sprite_registry, 0, sizeof(sprite_registry));
    sprite_count = 0;
    last_sprite_id = SPRITE_ID_INVALID;
    registry_status = REGISTRY_STATUS_READY;
    
    printk("Sprite Service: Registry cleared\n");
    return 0;
}

void sprite_service_get_statistics(uint16_t *total_sprites, uint16_t *free_slots, uint16_t *crc_errors)
{
    if (total_sprites) *total_sprites = sprite_count;
    if (free_slots) *free_slots = SPRITE_MAX_COUNT - sprite_count;
    if (crc_errors) *crc_errors = crc_error_count;
}
