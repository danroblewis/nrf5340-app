# BLE Testing Suite

This directory contains comprehensive test suites for the nRF5340 BLE Multi-Service Device, combining BLE protocol testing with real-time serial monitoring for complete validation.

## Test Framework Features

- **Enhanced BLE Testing**: Protocol validation with device-side verification
- **Serial Monitoring**: Real-time capture and pattern matching of device logs
- **Shared Resources**: Efficient connection reuse across test suites
- **Comprehensive Coverage**: Tests for all BLE services and device functionality

## Test Files

### Test Suites

- `test_ble_comprehensive.py` - All basic BLE services (Device Info, Control, Data, DFU)
- `test_wasm_service.py` - WASM service functionality (upload, execution, status)
- `test_sprite_service.py` - Sprite registry service (upload, download, verification)
- `test_mtu_negotiation.py` - Focused MTU negotiation testing

### Framework and Utilities

- `test_framework_with_serial.py` - Core enhanced testing framework
- `conftest.py` - Pytest configuration and shared fixtures
- `pytest_ble_demo.py` - Working demonstration of pytest approach
- `run_pytest_tests.py` - Enhanced test runner with options

## Quick Start

### Prerequisites

```bash
# Install dependencies
pip install -r ../requirements.txt

# Ensure device is connected and serial port is accessible
ls /dev/tty.usbmodem*
```

### Running Tests

All tests should be run from within the `tests/` directory:

```bash
cd tests

# Run all pytest-based tests
python run_pytest_tests.py

# Run specific test suite
python run_pytest_tests.py --suite wasm

# Run tests with specific markers
python run_pytest_tests.py --markers "not slow"

# Generate reports
python run_pytest_tests.py --output-dir results

# Run individual pytest test suites
python -m pytest test_wasm_service.py -v
python -m pytest test_ble_comprehensive.py -v
python -m pytest test_sprite_service.py -v

# Run focused tests
python -m pytest test_mtu_negotiation.py -v
```

## Test Configuration

### Device Settings

- **Device Name**: `Dan5340BLE`
- **Serial Port**: `/dev/tty.usbmodem0010500306563`
- **Baud Rate**: `115200`
- **Connection Timeout**: `15 seconds`

### Test Markers

- `@pytest.mark.wasm` - WASM service tests
- `@pytest.mark.sprite` - Sprite service tests  
- `@pytest.mark.ble` - General BLE tests
- `@pytest.mark.integration` - Integration tests
- `@pytest.mark.slow` - Long-running tests

## Test Architecture

### Shared Resources (Session-Scoped)

1. **Serial Monitor**: Started once, shared across all tests
2. **BLE Connection**: Single connection reused for efficiency
3. **Service Discovery**: Cached service/characteristic mappings

### Test Verification Levels

1. **BLE Protocol**: Response validation, data format checking
2. **Device Behavior**: Serial log pattern matching, state verification
3. **End-to-End**: Complete workflow validation

### Example Test Structure

```python
@pytest.mark.wasm
async def test_wasm_upload(self, ble_client, wasm_helper, test_result):
    """Test WASM binary upload with serial verification"""
    
    # Execute BLE operation
    await test_result.verify_ble_operation(
        wasm_helper.upload_binary, ble_client
    )
    
    # Verify device-side logs
    test_result.verify_serial_patterns([
        r"WASM Service: wasm_upload_handler called",
        r"WASM Service: Upload complete.*loading module",
        r"WASM Service: WASM module loaded successfully"
    ])
    
    # Assert overall success
    test_result.assert_success()
```

## Device Services Tested

### Device Information Service (0x180A)
- Manufacturer name, model number, firmware/hardware/software revisions

### Control Service (0xFFE0)  
- Command/response handling, status reporting

### Data Service (0xFFF0)
- Upload/download operations, round-trip verification

### DFU Service (0xFE59)
- Device firmware update commands and packet handling

### Sprite Service (0xFFF8)
- Sprite registry management, upload/download/verification

### WASM Service (0xFFF7)
- WebAssembly upload, compilation, and execution

## Troubleshooting

### Common Issues

1. **Serial Port Access**: Ensure no other applications are using the serial port
2. **Device Discovery**: Check device name and ensure BLE advertising is active
3. **Connection Timeouts**: Verify device is powered and within range
4. **Test Dependencies**: Install all required packages from `requirements_pytest.txt`

### Debug Options

```bash
# Verbose output
python run_pytest_tests.py --verbose

# Environment validation only
python run_pytest_tests.py --validate-only

# Single test with detailed logging
python -m pytest test_wasm_pytest.py::TestWASMStatus::test_status_read -v -s
```

## Contributing

When adding new tests:

1. Follow the existing test structure and naming conventions
2. Use appropriate pytest markers
3. Include both BLE and serial verification where applicable
4. Add comprehensive docstrings and logging
5. Update this README with new test descriptions
