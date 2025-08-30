#ifndef WASM_SERVICE_H
#define WASM_SERVICE_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @file wasm_service.h
 * @brief Custom WASM Service for receiving and executing WebAssembly code
 * 
 * This service provides:
 * - Multi-packet WASM bytecode upload
 * - WASM program execution with function calls
 * - Status and result retrieval
 */

/* ============================================================================
 * SERVICE AND CHARACTERISTIC UUIDs
 * ============================================================================ */

/* WASM Service UUID: 12345678-1234-5678-9abc-def012345006 */
#define WASM_SERVICE_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x9abc, 0xdef012345006)

/* WASM Upload Characteristic: 12345678-1234-5678-9abc-def012345016 */
#define WASM_UPLOAD_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x9abc, 0xdef012345016)

/* WASM Execute Characteristic: 12345678-1234-5678-9abc-def012345026 */
#define WASM_EXECUTE_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x9abc, 0xdef012345026)

/* WASM Status Characteristic: 12345678-1234-5678-9abc-def012345036 */
#define WASM_STATUS_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x9abc, 0xdef012345036)

/* WASM Result Characteristic: 12345678-1234-5678-9abc-def012345046 */
#define WASM_RESULT_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x9abc, 0xdef012345046)

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/* WASM memory configuration */
#define WASM_CODE_BUFFER_SIZE       (32 * 1024)    /* 32KB for WASM bytecode */
#define WASM_UPLOAD_CHUNK_SIZE      244             /* BLE packet size - headers */
#define WASM_FUNCTION_NAME_SIZE     32              /* Maximum function name length */
#define WASM_RESULT_DATA_SIZE       32              /* Maximum result data size */

/* ============================================================================
 * STATUS CODES
 * ============================================================================ */

/* WASM upload status */
#define WASM_STATUS_IDLE                0x00
#define WASM_STATUS_RECEIVING           0x01
#define WASM_STATUS_RECEIVED            0x02
#define WASM_STATUS_LOADED              0x03
#define WASM_STATUS_EXECUTING           0x04
#define WASM_STATUS_COMPLETE            0x05
#define WASM_STATUS_ERROR               0x06

/* WASM error codes */
#define WASM_ERROR_NONE                 0x00
#define WASM_ERROR_BUFFER_OVERFLOW      0x01
#define WASM_ERROR_INVALID_MAGIC        0x02
#define WASM_ERROR_LOAD_FAILED          0x03
#define WASM_ERROR_COMPILE_FAILED       0x04
#define WASM_ERROR_FUNCTION_NOT_FOUND   0x05
#define WASM_ERROR_EXECUTION_FAILED     0x06
#define WASM_ERROR_INVALID_PARAMS       0x07

/* Upload command codes */
#define WASM_CMD_START_UPLOAD           0x01
#define WASM_CMD_CONTINUE_UPLOAD        0x02
#define WASM_CMD_END_UPLOAD             0x03
#define WASM_CMD_RESET                  0x04

/* ============================================================================
 * PACKET STRUCTURES
 * ============================================================================ */

/**
 * @brief WASM upload packet structure
 * Used for uploading WASM bytecode in chunks
 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd;                               /* Upload command */
    uint8_t  sequence;                          /* Packet sequence number */
    uint16_t chunk_size;                        /* Size of data in this chunk */
    uint32_t total_size;                        /* Total WASM binary size (in first packet) */
    uint8_t  data[WASM_UPLOAD_CHUNK_SIZE];      /* WASM bytecode chunk */
} wasm_upload_packet_t;

/**
 * @brief WASM execute packet structure
 * Used for executing WASM functions
 */
typedef struct __attribute__((packed)) {
    char     function_name[WASM_FUNCTION_NAME_SIZE];  /* Function to call */
    uint32_t arg_count;                               /* Number of arguments */
    int32_t  args[4];                                 /* Function arguments (max 4) */
} wasm_execute_packet_t;

/**
 * @brief WASM status packet structure
 * Used for reporting current status and progress
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;                            /* Current WASM status */
    uint8_t  error_code;                        /* Last error code */
    uint16_t bytes_received;                    /* Bytes received so far */
    uint32_t total_size;                        /* Total expected size */
    uint32_t uptime;                            /* System uptime */
    uint8_t  reserved[6];                       /* Reserved for future use */
} wasm_status_packet_t;

/**
 * @brief WASM result packet structure
 * Used for returning execution results
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;                            /* Execution status */
    uint8_t  error_code;                        /* Error code if failed */
    int32_t  return_value;                      /* Function return value */
    uint32_t execution_time_us;                 /* Execution time in microseconds */
    uint8_t  result_data[WASM_RESULT_DATA_SIZE]; /* Additional result data */
} wasm_result_packet_t;

/* ============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * @brief Initialize the WASM service
 * @return 0 on success, negative error code on failure
 */
int wasm_service_init(void);

/**
 * @brief Handle BLE connection events
 * @param conn Connection handle
 * @param connected True if connected, false if disconnected
 */
void wasm_service_connection_event(struct bt_conn *conn, bool connected);

/**
 * @brief Get current WASM service status
 * @return Current status code
 */
uint8_t wasm_service_get_status(void);

/**
 * @brief Get current error code
 * @return Current error code
 */
uint8_t wasm_service_get_error_code(void);

/**
 * @brief Get number of bytes received
 * @return Number of bytes received
 */
uint16_t wasm_service_get_bytes_received(void);

/**
 * @brief Check if WASM code is loaded and ready for execution
 * @return True if ready, false otherwise
 */
bool wasm_service_is_ready(void);

/**
 * @brief Reset WASM service state and clear memory
 */
void wasm_service_reset(void);

/**
 * @brief Execute a WASM function by name
 * @param function_name Name of the function to call
 * @param args Array of arguments
 * @param arg_count Number of arguments
 * @param result Pointer to store the result
 * @return 0 on success, negative error code on failure
 */
int wasm_service_execute_function(const char *function_name, 
                                 const int32_t *args, 
                                 uint32_t arg_count, 
                                 int32_t *result);

/**
 * @brief Get the last execution result
 * @param result_packet Pointer to result packet to fill
 * @return 0 on success, negative error code on failure
 */
int wasm_service_get_last_result(wasm_result_packet_t *result_packet);

/**
 * @brief Validate WASM magic number and basic structure
 * @param data Pointer to WASM bytecode
 * @param size Size of the data
 * @return True if valid WASM format, false otherwise
 */
bool wasm_service_validate_magic(const uint8_t *data, size_t size);

#endif /* WASM_SERVICE_H */
