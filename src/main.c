#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "wasm3_wrapper.h"
#include "wasm_test_module.h"

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
static uint8_t received_data[256];
static uint8_t data_length = 0;

// wasm3 runtime instance
static wasm3_runtime_t wasm3_runtime;

// Callback for when the characteristic is written to
static ssize_t on_write(struct bt_conn *conn,
			const struct bt_gatt_attr *attr,
			const void *buf,
			uint16_t len,
			uint16_t offset,
			uint8_t flags)
{
	// Store the received data
	if (len <= sizeof(received_data)) {
		memcpy(received_data, buf, len);
		data_length = len;
		
		// Print received data to serial console
		printk("Received %d bytes: ", len);
		for (int i = 0; i < len; i++) {
			printk("%02x ", received_data[i]);
		}
		printk("\n");
		
		// Also print as ASCII if it's printable
		printk("ASCII: ");
		for (int i = 0; i < len; i++) {
			if (received_data[i] >= 32 && received_data[i] <= 126) {
				printk("%c", received_data[i]);
			} else {
				printk(".");
			}
		}
		printk("\n");
		
		// Try to execute the received data as WASM if it's valid
		if (len >= 4 && received_data[0] == 0x00 && received_data[1] == 0x61 && 
		    received_data[2] == 0x73 && received_data[3] == 0x6d) {
			printk("Valid WASM binary detected! Loading with wasm3...\n");
			
			// Load the received WASM data
			if (wasm3_load_module(&wasm3_runtime, received_data, len) == 0) {
				if (wasm3_compile_module(&wasm3_runtime) == 0) {
					// Try to call a function
					int result;
					if (wasm3_call_function(&wasm3_runtime, "main", NULL, 0, &result) == 0) {
						printk("WASM function executed successfully with wasm3, result: %d\n", result);
					}
				}
			}
		}
	} else {
		printk("Data too long (%d bytes), max is %d\n", len, sizeof(received_data));
	}
	
	return len;
}

// GATT service definition using BT_GATT_SERVICE macro
BT_GATT_SERVICE_DEFINE(custom_service,
	BT_GATT_PRIMARY_SERVICE(&custom_service_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&custom_char_uuid.uuid,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, on_write, NULL),
);

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}
	printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
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

	// Load test WASM module
	printk("Loading test WASM module...\n");
	err = wasm3_load_module(&wasm3_runtime, test_wasm_module, TEST_WASM_MODULE_SIZE);
	if (err) {
		printk("WASM module load failed (err %d)\n", err);
		return 0;
	}

	// Compile test WASM module
	printk("Compiling test WASM module...\n");
	err = wasm3_compile_module(&wasm3_runtime);
	if (err) {
		printk("WASM module compilation failed (err %d)\n", err);
		return 0;
	}

	// Execute test WASM module
	printk("Executing test WASM module...\n");
	int result;
	err = wasm3_call_function(&wasm3_runtime, "test", NULL, 0, &result);
	if (err) {
		printk("WASM execution failed (err %d)\n", err);
		return 0;
	}

	printk("wasm3 integration complete!\n");
	printk("Test function result: %d\n", result);

	while (1) {
		k_sleep(K_SECONDS(10));
		printk("BLE device running with wasm3 support...\n");
		printk("Send WASM binaries via BLE to execute them with wasm3!\n");
	}
}