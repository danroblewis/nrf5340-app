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

## Project Structure

- `src/main.c` - Main application code with BLE peripheral implementation
- `prj.conf` - Zephyr configuration (BLE, serial console, etc.)
- `CMakeLists.txt` - CMake build configuration
- `build.sh` - Build script
- `flash.sh` - Flash script
- `tests/` - Comprehensive BLE testing suite