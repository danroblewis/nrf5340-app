# nRF5340 BLE Peripheral Example

A simple Bluetooth Low Energy peripheral example for the nRF5340 microcontroller.

## Features

- BLE peripheral device with advertising
- Custom device name: "Dan5340BLE"
- Serial console output via `printk`
- Connection status callbacks

## Build Instructions

### Prerequisites

- Nordic Connect SDK (NCS) installed at `~/ncs`
- Environment variables set (run `source ~/ncs/zephyr/zephyr-env.sh`)

### Build

From the project directory:

```bash
./build.sh
```

This will build both the CPUAPP (application core) and CPUNET (network core) images.

### Flash

After building, flash to your nRF5340 device:

```bash
./flash.sh
```

## Manual Commands

If you prefer to run commands manually:

```bash
# Build
cd ~/ncs
west build -p always -b nrf5340dk_nrf5340_cpuapp -s /path/to/your/project

# Flash
west flash
```

## Serial Console

To view serial output, connect to the device's serial port:

```bash
screen /dev/tty.usbmodem* 115200
```

## Troubleshooting

- **Build fails with CMake errors**: The NCS installation has been fixed to work with CMake 4.1.0
- **BLE not advertising**: Ensure both cores are flashed (the build script handles this automatically)
- **Serial output not visible**: Check your serial port configuration and baud rate

## Testing

The project includes comprehensive BLE testing with both individual scripts and pytest-based test suites. All tests are organized in the `tests/` directory.

### Quick Start

```bash
# Install dependencies
pip install -r requirements.txt

# Change to tests directory
cd tests

# Run all pytest-based tests (recommended)
python run_pytest_tests.py

# Run specific test suite
python run_pytest_tests.py --suite wasm

# Run individual test files
python test_wasm_service_basic.py
```

### Test Suites

#### Pytest-Based Tests (Recommended)
- **Shared Resources**: Single BLE connection and serial monitor across all tests
- **Enhanced Debugging**: Real-time serial log capture and pattern matching
- **Better Organization**: Proper test discovery, markers, and reporting

#### Enhanced Tests with Serial Monitoring
- **Complete Validation**: Tests both BLE protocol and device-side behavior
- **Serial Pattern Matching**: Verify device logs during operations
- **Comprehensive Coverage**: Full end-to-end workflow validation

#### Individual Test Scripts
- **Original Tests**: Standalone scripts for specific service testing
- **Quick Testing**: Run individual services without setup overhead

See `tests/README.md` for detailed testing documentation.

## WASM Development

This device supports uploading and executing WebAssembly (WASM) modules via BLE. You can write applications in Rust and compile them to run on the device.

### Installing Rust and WASM Tools

1. **Install Rust:**
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   source ~/.cargo/env
   ```

2. **Add WASM target:**
   ```bash
   rustup target add wasm32-unknown-unknown
   ```

3. **Install wasm-pack (optional, for advanced features):**
   ```bash
   curl https://rustwasm.github.io/wasm-pack/installer/init.sh -sSf | sh
   ```

### Creating a WASM Application

1. **Create a new Rust library:**
   ```bash
   cargo new --lib my_wasm_app
   cd my_wasm_app
   ```

2. **Edit `Cargo.toml`:**
   ```toml
   [package]
   name = "my_wasm_app"
   version = "0.1.0"
   edition = "2021"

   [lib]
   crate-type = ["cdylib"]

   [profile.release]
   opt-level = "s"  # Optimize for size
   ```

3. **Write your application in `src/lib.rs`:**
   ```rust
   // Export functions that can be called from the device
   #[no_mangle]
   pub extern "C" fn fibonacci(n: i32) -> i32 {
       if n <= 1 {
           return n;
       }
       
       let mut a = 0;
       let mut b = 1;
       
       for _ in 2..=n {
           let temp = a + b;
           a = b;
           b = temp;
       }
       
       b
   }

   #[no_mangle]
   pub extern "C" fn add(a: i32, b: i32) -> i32 {
       a + b
   }
   ```

4. **Compile to WASM:**
   ```bash
   cargo build --target wasm32-unknown-unknown --release
   ```

5. **Find your WASM binary:**
   ```bash
   ls target/wasm32-unknown-unknown/release/*.wasm
   ```

### Uploading WASM to Device

Use the Python test scripts to upload and execute your WASM:

```bash
cd tests
python test_wasm_service_basic.py --wasm-file ../path/to/your/app.wasm
```

Or use the BLE testing framework to upload your WASM module and call functions on it.

### WASM Function Interface

Your exported functions should follow C calling conventions:
- **Parameters**: Up to 4 `i32` parameters
- **Return**: Single `i32` value
- **Function names**: Must be null-terminated strings (max 32 chars)

Example function signatures that work with the device:
```rust
#[no_mangle] pub extern "C" fn my_function() -> i32 { ... }
#[no_mangle] pub extern "C" fn add_numbers(a: i32, b: i32) -> i32 { ... }
#[no_mangle] pub extern "C" fn process_data(x: i32, y: i32, z: i32, w: i32) -> i32 { ... }
```

## Project Structure

- `src/main.c` - Main application code with BLE peripheral implementation
- `src/services/` - BLE service implementations (WASM, data, control, etc.)
- `prj.conf` - Zephyr configuration (BLE, serial console, etc.)
- `CMakeLists.txt` - CMake build configuration
- `build.sh` - Build script
- `flash.sh` - Flash script
- `tests/` - Comprehensive BLE testing suite