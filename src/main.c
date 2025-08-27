#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "wasm3_wrapper.h"


// Custom service UUID (randomly generated)
static const struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(
	0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x12, 0x34,
	0x12, 0x34, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc
);

// Custom characteristic UUID (randomly generated)
static const struct bt_uuid_128 custom_char_uuid = BT_UUID_INIT_128(
	0x87, 0x65, 0x43, 0x21, 0x43, 0x21, 0x43, 0x21,
	0x43, 0x21, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21
);

// Buffer to store received data
static uint8_t received_data[4096];  // Increased buffer size
static uint16_t data_length = 0;
static uint16_t total_received = 0;
static bool receiving_wasm = false;

// wasm3 runtime instance
static wasm3_runtime_t wasm3_runtime;

// Callback for when the characteristic is read
static ssize_t on_read(struct bt_conn *conn,
		       const struct bt_gatt_attr *attr,
		       void *buf,
		       uint16_t len,
		       uint16_t offset)
{
	printk("Characteristic read request - offset: %d, len: %d\n", offset, len);
	printk("Connection handle: %d\n", bt_conn_index(conn));
	
	// Return a simple message
	const char *msg = "WASM3 Ready";
	uint16_t msg_len = strlen(msg);
	
	if (offset >= msg_len) {
		printk("Invalid offset: %d >= %d\n", offset, msg_len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	
	len = (len < (msg_len - offset)) ? len : (msg_len - offset);
	memcpy(buf, msg + offset, len);
	
	printk("Read response sent: %d bytes\n", len);
	return len;
}

// Callback for when the characteristic is written to
static ssize_t on_write(struct bt_conn *conn,
			const struct bt_gatt_attr *attr,
			const void *buf,
			uint16_t len,
			uint16_t offset,
			uint8_t flags)
{
	printk("\n=== BLE WRITE RECEIVED ===\n");
	printk("Characteristic write request - offset: %d, len: %d, flags: %d\n", offset, len, flags);
	printk("Connection handle: %d\n", bt_conn_index(conn));
	
	// Check if this is the start of a new WASM transmission
	if (len >= 4 && !receiving_wasm && 
	    ((uint8_t*)buf)[0] == 0x00 && ((uint8_t*)buf)[1] == 0x61 && 
	    ((uint8_t*)buf)[2] == 0x73 && ((uint8_t*)buf)[3] == 0x6d) {
		printk("New WASM transmission started\n");
		receiving_wasm = true;
		total_received = 0;
		data_length = 0;
	}
	
	// Store the received data chunk
	if (receiving_wasm && (total_received + len) <= sizeof(received_data)) {
		memcpy(received_data + total_received, buf, len);
		total_received += len;
		data_length = total_received;
		
		printk("Chunk received: %d bytes, total: %d bytes\n", len, total_received);
		
		// Print received data to serial console
		printk("Received %d bytes: ", len);
		for (int i = 0; i < len; i++) {
			printk("%02x ", ((uint8_t*)buf)[i]);
		}
		printk("\n");
		
		// // Sleep to ensure debug messages are printed
		// printk("Sleeping for 1 second to ensure debug output...\n");
		// k_sleep(K_SECONDS(1));
		
		// Try to execute the accumulated WASM data if we have enough
		if (total_received >= 4 && received_data[0] == 0x00 && received_data[1] == 0x61 && 
		    received_data[2] == 0x73 && received_data[3] == 0x6d) {
			printk("Valid WASM binary detected! Loading with wasm3...\n");
			printk("Total WASM size: %d bytes\n", total_received);
			
			// Load the received WASM data
			if (wasm3_load_module(&wasm3_runtime, received_data, total_received) == 0) {
				if (wasm3_compile_module(&wasm3_runtime) == 0) {
					// Try to call the "add" function
					int result;
					if (wasm3_call_function(&wasm3_runtime, "add", NULL, 0, &result) == 0) {
						printk("WASM function 'add' executed successfully with wasm3, result: %d\n", result);
					}
				}
			}
			
			// Reset for next transmission
			receiving_wasm = false;
			total_received = 0;
			data_length = 0;
		}
	} else if (!receiving_wasm) {
		printk("Received non-WASM data: %d bytes\n", len);
	} else {
		printk("WASM data too long (%d + %d bytes), max is %d\n", total_received, len, sizeof(received_data));
	}
	
	printk("=== BLE WRITE COMPLETED ===\n");
	return len;
}

// GATT service definition using BT_GATT_SERVICE macro
BT_GATT_SERVICE_DEFINE(custom_service,
	BT_GATT_PRIMARY_SERVICE(&custom_service_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&custom_char_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       on_read, on_write, NULL),
);

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}
	printk("Connected\n");
	printk("Connection handle: %d\n", bt_conn_index(conn));
	printk("Remote address type: %d\n", bt_conn_get_dst(conn)->type);
	
	printk("Connection established - MTU should be configured via prj.conf\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
	printk("Connection handle: %d\n", bt_conn_index(conn));
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

// Note: MTU monitoring would require bt_gatt_cb which may not be available
// in this Zephyr version. We'll rely on the MTU configuration in prj.conf.

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");
	printk("Custom service automatically registered\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, NULL, 0, NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
	printk("Device name: Dan5340BLE\n");
	printk("Look for a writable characteristic in the custom service\n");
}

int main(void)
{
	int err;
	wasm3_config_t wasm3_config = {
		.stack_size = 8192,
		.heap_size = 16384,
		.enable_tracing = false
	};

	printk("\n");
	printk("========================================\n");
	printk("nRF5340 BLE + wasm3 Integration Starting\n");
	printk("Build: %s %s\n", __DATE__, __TIME__);
	printk("========================================\n");
	printk("Starting BLE Peripheral with wasm3 Integration\n");
	printk("wasm3: Fast WebAssembly Interpreter\n");

	// Initialize the Bluetooth subsystem
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	// Register connection callbacks
	bt_conn_cb_register(&conn_callbacks);

	// Initialize wasm3 runtime
	printk("Initializing wasm3 runtime...\n");
	err = wasm3_init(&wasm3_runtime, &wasm3_config);
	if (err) {
		printk("wasm3 runtime init failed (err %d)\n", err);
		return 0;
	}

	printk("wasm3 runtime initialized successfully\n");

	while (1) {
		k_sleep(K_SECONDS(10));
		printk(".\n");
	}
}