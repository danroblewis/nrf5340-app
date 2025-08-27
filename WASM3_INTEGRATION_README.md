# wasm3 Integration for nRF5340

## Overview

This project integrates [wasm3](https://github.com/wasm3/wasm3) (a fast WebAssembly interpreter) with the nRF5340 microcontroller running Zephyr RTOS. The goal is to enable dynamic WASM code execution on embedded devices via BLE.

## Current Status

**Phase 1: Placeholder Integration (COMPLETE)**
- ✅ Basic wasm3 wrapper interface created
- ✅ BLE integration working
- ✅ WASM binary detection via BLE
- ✅ Placeholder wasm3 runtime functions
- ✅ Build system working

**Phase 2: Full wasm3 Integration (IN PROGRESS)**
- 🔄 Integrating actual wasm3 library
- 🔄 Implementing real WASM parsing and execution
- 🔄 Adding proper error handling

## Architecture

### Files Structure
```
src/
├── main.c              # Main application with BLE + wasm3 integration
├── wasm3_wrapper.h     # wasm3 wrapper interface
├── wasm3_wrapper.c     # wasm3 wrapper implementation
├── wasm_interpreter.c  # Legacy mock interpreter (can be removed)
└── wasm_test_module.h  # Test WASM bytecode
```

### wasm3 Wrapper Interface

The `wasm3_wrapper.h` provides a clean interface for:
- **Runtime Management**: `wasm3_init()`, `wasm3_cleanup()`
- **Module Loading**: `wasm3_load_module()`, `wasm3_compile_module()`
- **Function Execution**: `wasm3_call_function()`
- **Error Handling**: `wasm3_print_error()`

### BLE Integration

- **Device Name**: `Dan5340BLE`
- **Custom Service**: Randomly generated UUID starting with `12345678`
- **Writable Characteristic**: Accepts WASM binaries via BLE
- **WASM Detection**: Automatically detects valid WASM binaries (magic number `0x00 0x61 0x73 0x6d`)

## Building and Flashing

```bash
# Build the project
./build.sh

# Flash to device
./flash.sh
```

## Testing

### 1. Serial Console
Connect to the device's serial port to see debug output:
```bash
screen /dev/tty.usbmodem* 115200
```

### 2. BLE Testing
Use the provided Python script to test WASM integration:
```bash
python3 test_wasm3_integration.py
```

This script will:
- Scan for the `Dan5340BLE` device
- Connect to the custom service
- Send a minimal WASM binary
- Verify the wasm3 wrapper processes it

## Next Steps

### Immediate (Phase 2)
1. **Integrate Actual wasm3 Library**
   - Copy essential wasm3 source files to project
   - Update CMakeLists.txt to build wasm3
   - Replace placeholder functions with real implementations

2. **Enhance WASM Support**
   - Implement proper WASM parsing
   - Add function lookup and execution
   - Support for basic WASM operations

### Future Enhancements
1. **WASM Features**
   - Memory management
   - Import/export functions
   - Error handling and debugging

2. **BLE Enhancements**
   - Larger WASM binary support
   - Streaming WASM uploads
   - Result return via BLE

3. **Performance**
   - WASM execution optimization
   - Memory usage optimization
   - Real-time constraints

## Why wasm3?

wasm3 was chosen over alternatives like WAMR because:

- **Simplicity**: Clean, focused codebase
- **Portability**: Runs on many architectures including ARM Cortex-M
- **Performance**: Fast interpreter design
- **Embedded Friendly**: Small memory footprint
- **Active Development**: Well-maintained project

## Troubleshooting

### Build Issues
- Ensure NCS environment is sourced: `source ~/ncs/zephyr/zephyr-env.sh`
- Check CMake version compatibility
- Verify all dependencies are available

### BLE Issues
- Ensure device is advertising: `Dan5340BLE` should be visible
- Check UUID byte order (Zephyr may reverse 128-bit UUIDs)
- Verify characteristic properties (must be writable)

### WASM Issues
- Check WASM binary validity (magic number, version)
- Monitor serial console for wasm3 wrapper messages
- Verify function names in WASM modules

## References

- [wasm3 GitHub Repository](https://github.com/wasm3/wasm3)
- [wasm3 Documentation](https://github.com/wasm3/wasm3#readme)
- [Zephyr BLE Documentation](https://docs.zephyrproject.org/latest/connectivity/bluetooth/index.html)
- [nRF5340 Development Guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
