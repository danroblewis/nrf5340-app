#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <string.h>

/* ============================================================================
 * DEVICE INFORMATION SERVICE (Standard 0x180A)
 * ============================================================================ */

static const char manufacturer_name[] = "Nordic Semiconductor";
static const char model_number[] = "nRF5340-DK";
static const char firmware_revision[] = "v1.0.0";
static const char hardware_revision[] = "PCA10095";
static const char software_revision[] = "Zephyr 3.5.0";

static ssize_t read_manufacturer_name(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           manufacturer_name, strlen(manufacturer_name));
}

static ssize_t read_model_number(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           model_number, strlen(model_number));
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
                           hardware_revision, strlen(hardware_revision));
}

static ssize_t read_software_revision(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           software_revision, strlen(software_revision));
}

/* Device Information Service Definition */
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
 * DEVICE FIRMWARE UPDATE SERVICE (Standard 0xFE59) - Mock Implementation
 * ============================================================================ */

#define DFU_SERVICE_UUID            BT_UUID_16(0xFE59)
#define DFU_CONTROL_POINT_UUID      BT_UUID_128(BT_UUID_128_ENCODE(0x8EC90001, 0xF315, 0x4F60, 0x9FB8, 0x838830DAEA50))
#define DFU_PACKET_UUID             BT_UUID_128(BT_UUID_128_ENCODE(0x8EC90002, 0xF315, 0x4F60, 0x9FB8, 0x838830DAEA50))

/* DFU State */
static uint8_t dfu_state = 0; // 0=idle, 1=ready, 2=updating
static uint32_t dfu_bytes_received = 0;
static struct bt_conn *dfu_conn = NULL;

/* DFU Control Point Commands */
#define DFU_CMD_START_DFU           0x01
#define DFU_CMD_INITIALIZE_DFU      0x02
#define DFU_CMD_RECEIVE_FW          0x03
#define DFU_CMD_VALIDATE_FW         0x04
#define DFU_CMD_ACTIVATE_N_RESET    0x05

/* DFU Response Codes */
#define DFU_RSP_SUCCESS             0x01
#define DFU_RSP_INVALID_STATE       0x02
#define DFU_RSP_NOT_SUPPORTED       0x03
#define DFU_RSP_DATA_SIZE_EXCEEDS   0x04
#define DFU_RSP_CRC_ERROR           0x05
#define DFU_RSP_OPERATION_FAILED    0x06

static void dfu_control_point_indicate(uint8_t opcode, uint8_t response_code)
{
    if (!dfu_conn) {
        return;
    }
    
    uint8_t response[3] = {0x60, opcode, response_code}; // 0x60 = Response opcode
    
    printk("DFU: Sending indication - OpCode: 0x%02x, Response: 0x%02x\n", 
           opcode, response_code);
    
    /* In real implementation, would use bt_gatt_indicate() */
    /* For mock, we just print the response */
}

static ssize_t dfu_control_point_write(struct bt_conn *conn,
                                      const struct bt_gatt_attr *attr,
                                      const void *buf, uint16_t len,
                                      uint16_t offset, uint8_t flags)
{
    const uint8_t *data = (const uint8_t *)buf;
    
    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    printk("DFU: Control Point command received: 0x%02x\n", data[0]);
    
    dfu_conn = conn;
    
    switch (data[0]) {
    case DFU_CMD_START_DFU:
        printk("DFU: Start DFU command\n");
        dfu_state = 1;
        dfu_bytes_received = 0;
        dfu_control_point_indicate(DFU_CMD_START_DFU, DFU_RSP_SUCCESS);
        break;
        
    case DFU_CMD_INITIALIZE_DFU:
        printk("DFU: Initialize DFU command\n");
        if (dfu_state == 1) {
            dfu_control_point_indicate(DFU_CMD_INITIALIZE_DFU, DFU_RSP_SUCCESS);
        } else {
            dfu_control_point_indicate(DFU_CMD_INITIALIZE_DFU, DFU_RSP_INVALID_STATE);
        }
        break;
        
    case DFU_CMD_RECEIVE_FW:
        printk("DFU: Receive firmware command\n");
        dfu_state = 2;
        dfu_control_point_indicate(DFU_CMD_RECEIVE_FW, DFU_RSP_SUCCESS);
        break;
        
    case DFU_CMD_VALIDATE_FW:
        printk("DFU: Validate firmware command\n");
        printk("DFU: Mock validation - received %d bytes\n", dfu_bytes_received);
        dfu_control_point_indicate(DFU_CMD_VALIDATE_FW, DFU_RSP_SUCCESS);
        break;
        
    case DFU_CMD_ACTIVATE_N_RESET:
        printk("DFU: Activate and reset command (mock - not actually resetting)\n");
        dfu_state = 0;
        dfu_control_point_indicate(DFU_CMD_ACTIVATE_N_RESET, DFU_RSP_SUCCESS);
        break;
        
    default:
        printk("DFU: Unknown command: 0x%02x\n", data[0]);
        dfu_control_point_indicate(data[0], DFU_RSP_NOT_SUPPORTED);
        break;
    }
    
    return len;
}

static ssize_t dfu_packet_write(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len,
                               uint16_t offset, uint8_t flags)
{
    if (dfu_state != 2) {
        printk("DFU: Packet received but not in receive state\n");
        return BT_GATT_ERR(BT_ATT_ERR_REQUEST_NOT_SUPPORTED);
    }
    
    dfu_bytes_received += len;
    printk("DFU: Firmware packet received: %d bytes (total: %d)\n", len, dfu_bytes_received);
    
    /* Mock processing - just count bytes */
    
    return len;
}

/* DFU Service Definition */
BT_GATT_SERVICE_DEFINE(dfu_service,
    BT_GATT_PRIMARY_SERVICE(DFU_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(DFU_CONTROL_POINT_UUID,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
                          BT_GATT_PERM_WRITE,
                          NULL, dfu_control_point_write, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(DFU_PACKET_UUID,
                          BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, dfu_packet_write, NULL),
);

/* ============================================================================
 * CUSTOM CONTROL SERVICE
 * ============================================================================ */

#define CONTROL_SERVICE_UUID        BT_UUID_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x123456789ABC))
#define CONTROL_COMMAND_UUID        BT_UUID_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x123456789AC0))
#define CONTROL_RESPONSE_UUID       BT_UUID_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x123456789AC1))
#define CONTROL_STATUS_UUID         BT_UUID_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x123456789AC2))

/* Control Service State */
static uint8_t device_status = 0; // 0=idle, 1=busy, 2=error
static uint8_t last_response[64];
static uint16_t last_response_len = 0;
static struct bt_conn *control_conn = NULL;

/* Control Commands */
#define CMD_GET_STATUS      0x01
#define CMD_RESET_DEVICE    0x02
#define CMD_SET_CONFIG      0x03
#define CMD_GET_VERSION     0x04

static void control_notify_response(void)
{
    if (!control_conn || last_response_len == 0) {
        return;
    }
    
    printk("Control: Notifying response (%d bytes)\n", last_response_len);
    /* In real implementation, would use bt_gatt_notify() */
}

static ssize_t control_command_write(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags)
{
    const uint8_t *data = (const uint8_t *)buf;
    
    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    printk("Control: Command received: 0x%02x\n", data[0]);
    
    control_conn = conn;
    
    switch (data[0]) {
    case CMD_GET_STATUS:
        printk("Control: Get status command\n");
        last_response[0] = CMD_GET_STATUS;
        last_response[1] = 0x00; // Success
        last_response[2] = device_status;
        last_response_len = 3;
        control_notify_response();
        break;
        
    case CMD_RESET_DEVICE:
        printk("Control: Reset device command (mock)\n");
        device_status = 0; // Reset to idle
        last_response[0] = CMD_RESET_DEVICE;
        last_response[1] = 0x00; // Success
        last_response_len = 2;
        control_notify_response();
        break;
        
    case CMD_SET_CONFIG:
        if (len >= 2) {
            printk("Control: Set config command (value: 0x%02x)\n", data[1]);
            last_response[0] = CMD_SET_CONFIG;
            last_response[1] = 0x00; // Success
            last_response_len = 2;
        } else {
            last_response[0] = CMD_SET_CONFIG;
            last_response[1] = 0x01; // Error - insufficient data
            last_response_len = 2;
        }
        control_notify_response();
        break;
        
    case CMD_GET_VERSION:
        printk("Control: Get version command\n");
        last_response[0] = CMD_GET_VERSION;
        last_response[1] = 0x00; // Success
        last_response[2] = 1; // Major
        last_response[3] = 0; // Minor
        last_response[4] = 0; // Patch
        last_response_len = 5;
        control_notify_response();
        break;
        
    default:
        printk("Control: Unknown command: 0x%02x\n", data[0]);
        last_response[0] = data[0];
        last_response[1] = 0xFF; // Error - unknown command
        last_response_len = 2;
        control_notify_response();
        break;
    }
    
    return len;
}

static ssize_t control_response_read(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    void *buf, uint16_t len, uint16_t offset)
{
    printk("Control: Response read request\n");
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           last_response, last_response_len);
}

static ssize_t control_status_read(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  void *buf, uint16_t len, uint16_t offset)
{
    uint8_t status_data[4] = {
        device_status,
        (uint8_t)(k_uptime_get() & 0xFF),
        (uint8_t)((k_uptime_get() >> 8) & 0xFF),
        (uint8_t)((k_uptime_get() >> 16) & 0xFF)
    };
    
    printk("Control: Status read request (status: %d)\n", device_status);
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           status_data, sizeof(status_data));
}

/* Control Service Definition */
BT_GATT_SERVICE_DEFINE(control_service,
    BT_GATT_PRIMARY_SERVICE(CONTROL_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(CONTROL_COMMAND_UUID,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, control_command_write, NULL),
    BT_GATT_CHARACTERISTIC(CONTROL_RESPONSE_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          control_response_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(CONTROL_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          control_status_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================================
 * CUSTOM DATA SERVICE
 * ============================================================================ */

#define DATA_SERVICE_UUID           BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ABCD))
#define DATA_UPLOAD_UUID            BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ACD0))
#define DATA_DOWNLOAD_UUID          BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ACD1))
#define DATA_TRANSFER_STATUS_UUID   BT_UUID_128(BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x8765, 0x4321, 0x87654321ACD2))

/* Data Service State */
static uint8_t data_buffer[1024];
static uint16_t data_buffer_size = 0;
static uint8_t transfer_status = 0; // 0=idle, 1=receiving, 2=complete, 3=error

static ssize_t data_upload_write(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len,
                                uint16_t offset, uint8_t flags)
{
    const uint8_t *data = (const uint8_t *)buf;
    
    printk("Data: Upload received %d bytes\n", len);
    
    if (data_buffer_size + len > sizeof(data_buffer)) {
        printk("Data: Buffer overflow, resetting\n");
        data_buffer_size = 0;
        transfer_status = 3; // Error
        return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }
    
    memcpy(data_buffer + data_buffer_size, data, len);
    data_buffer_size += len;
    transfer_status = 1; // Receiving
    
    printk("Data: Total received: %d bytes\n", data_buffer_size);
    
    /* Check for end marker or complete message */
    if (len < 20) { // Assume end of transmission if less than MTU
        transfer_status = 2; // Complete
        printk("Data: Transfer complete\n");
        
        /* Process received data */
        printk("Data: Processing %d bytes of data\n", data_buffer_size);
        
        /* Mock processing - just echo first few bytes */
        if (data_buffer_size > 0) {
            printk("Data: First bytes: ");
            for (int i = 0; i < (data_buffer_size > 8 ? 8 : data_buffer_size); i++) {
                printk("%02x ", data_buffer[i]);
            }
            printk("\n");
        }
    }
    
    return len;
}

static ssize_t data_download_read(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
    const char *sample_data = "Sample data from nRF5340 device";
    uint16_t sample_len = strlen(sample_data);
    
    printk("Data: Download request (offset: %d, len: %d)\n", offset, len);
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           sample_data, sample_len);
}

static ssize_t data_transfer_status_read(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        void *buf, uint16_t len, uint16_t offset)
{
    uint8_t status_data[6] = {
        transfer_status,
        (uint8_t)(data_buffer_size & 0xFF),
        (uint8_t)((data_buffer_size >> 8) & 0xFF),
        0x00, // Reserved
        0x00, // Reserved
        0x00  // Reserved
    };
    
    printk("Data: Transfer status read (status: %d, size: %d)\n", 
           transfer_status, data_buffer_size);
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                           status_data, sizeof(status_data));
}

/* Data Service Definition */
BT_GATT_SERVICE_DEFINE(data_service,
    BT_GATT_PRIMARY_SERVICE(DATA_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(DATA_UPLOAD_UUID,
                          BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                          BT_GATT_PERM_WRITE,
                          NULL, data_upload_write, NULL),
    BT_GATT_CHARACTERISTIC(DATA_DOWNLOAD_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          data_download_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(DATA_TRANSFER_STATUS_UUID,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          data_transfer_status_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

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
    
    device_status = 1; // Device is now busy (connected)
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    
    printk("Disconnected from %s (reason %u)\n", addr, reason);
    
    /* Reset connection pointers */
    if (conn == dfu_conn) {
        dfu_conn = NULL;
    }
    if (conn == control_conn) {
        control_conn = NULL;
    }
    
    device_status = 0; // Device is now idle
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");
    printk("Services registered:\n");
    printk("  - Device Information Service (0x180A)\n");
    printk("  - Device Firmware Update Service (0xFE59)\n");
    printk("  - Custom Control Service\n");
    printk("  - Custom Data Service\n");

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, NULL, 0, NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising successfully started\n");
    printk("Device name: nRF5340-BLE-Multi-Service\n");
}

/* ============================================================================
 * MAIN APPLICATION
 * ============================================================================ */

int main(void)
{
    int err;

    printk("\n");
    printk("========================================\n");
    printk("nRF5340 Multi-Service BLE Device\n");
    printk("Build: %s %s\n", __DATE__, __TIME__);
    printk("========================================\n");
    printk("Industry-Standard BLE Implementation\n");
    printk("Multiple Services and Characteristics\n");
    printk("========================================\n");

    /* Initialize the Bluetooth subsystem */
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    /* Register connection callbacks */
    bt_conn_cb_register(&conn_callbacks);

    printk("BLE device initialized successfully\n");
    printk("Waiting for connections...\n");

    /* Main loop - periodic status updates */
    while (1) {
        k_sleep(K_SECONDS(30));
        printk("Status: Device=%s, Uptime=%lld seconds\n",
               device_status == 0 ? "idle" : 
               device_status == 1 ? "connected" : "error",
               k_uptime_get() / 1000);
    }

    return 0;
}