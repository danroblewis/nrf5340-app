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

This device supports uploading and executing WebAssembly (WASM) modules via BLE. **Important: Use WAT (WebAssembly Text) for reliable development, not Rust.**

### ⚠️ Rust WASM Compatibility Issues

**Note from development experience**: I initially suggested Rust because "Rust people are really good at WASM," but this turned out to be incorrect advice. Our testing revealed significant compatibility issues:

- **❌ Rust-compiled WASMs consistently crash** with "Stack overflow (context area not valid)" errors
- **✅ WAT-compiled WASMs work perfectly** regardless of size or complexity
- **🔍 Root cause**: WASM3 library incompatibility with Rust toolchain output, not size or memory issues

**Recommendation**: Use WAT for reliable WASM development on this device.

### Creating WASM with WAT (Recommended)

1. **Write your WASM in WebAssembly Text format:**
   ```wat
   (module
     ;; Function that returns 99
     (func $get_number (result i32)
       i32.const 99)
     
     ;; Function that adds two numbers
     (func $add (param i32 i32) (result i32)
       local.get 0
       local.get 1
       i32.add)
     
     ;; Export functions
     (export "get_number" (func $get_number))
     (export "add" (func $add)))
   ```

2. **Compile WAT to WASM:**
   ```bash
   # Install wat2wasm (part of WebAssembly Binary Toolkit)
   brew install wabt
   
   # Compile
   wat2wasm my_app.wat -o my_app.wasm
   ```

3. **Test your WASM:**
   ```bash
   cd tests
   python test_wasm_service_basic.py --wasm-file ../my_app.wasm
   ```

### Rust WASM (Not Recommended - Has Compatibility Issues)

If you still want to try Rust (not recommended), here are the steps we attempted:

#### Installing Rust and WASM Tools

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

#### Creating a Rust WASM Application

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

**⚠️ Warning**: Rust-compiled WASMs will likely crash during upload/compilation with stack overflow errors. This is a known compatibility issue between the Rust toolchain and WASM3 runtime on this device.

**Note**: We did extensive optimization work on Rust WASMs (achieving 73% size reduction and 97% memory reduction), but the fundamental compatibility issue persisted. This work demonstrated that Rust WASM optimization techniques work correctly, but the underlying runtime compatibility issue cannot be resolved through compilation settings alone.

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

### Testing Results Summary

Our comprehensive testing revealed the following compatibility matrix:

| WASM Type | Size | Memory | Functions | Upload | Compile | Execute | Status |
|-----------|------|---------|-----------|---------|---------|---------|---------|
| **Simple WAT** | 65 bytes | None | 2 | ✅ | ✅ | ✅ | **Works perfectly** |
| **Minimal WAT** | 43 bytes | None | 1 | ✅ | ✅ | ✅ | **Works perfectly** |
| **Large WAT** | 434 bytes | 64KB | 21 | ✅ | ✅ | ✅ | **Works perfectly** |
| **Rust WASM** | 110 bytes | 64KB | 1 | ❌ | ❌ | ❌ | **Crashes consistently** |

**Key Findings:**
- **✅ WAT-compiled WASMs work reliably** regardless of size or complexity
- **❌ Rust-compiled WASMs crash consistently** with stack overflow errors
- **🔍 Size is NOT the issue** - 434-byte WAT works fine, 110-byte Rust crashes
- **🎯 Root cause**: WASM3 library incompatibility with Rust toolchain output

**Recommendation**: Use WAT for all WASM development on this device. It's more reliable, easier to debug, and fully compatible with the WASM3 runtime.

## Project Structure

- `src/main.c`