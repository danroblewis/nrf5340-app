# BLE Testing Guide for nRF5340

This guide explains how to test the BLE communication between your computer and the nRF5340 device.

## Prerequisites

### 1. Hardware Setup
- nRF5340 device flashed with the BLE peripheral code
- Computer with Bluetooth capability
- USB cable for serial console monitoring

### 2. Software Setup
- Python 3.7+ installed
- Required Python packages (see requirements.txt)

## Installation

### Install Python Dependencies
```bash
pip install -r requirements.txt
```

## Testing Methods

### Method 1: Python Script (Recommended)

The Python script using the `bleak` library is the most reliable method on macOS:

```bash
python3 test_ble_communication.py
```

**What it does:**
- Scans for BLE devices
- Connects to "Dan5340BLE"
- Writes test data to the writable characteristic
- Uses the correct discovered UUIDs for reliable communication

### Method 2: Mobile App

Use the **nRF Connect** mobile app:
1. Download from App Store/Google Play
2. Scan for "Dan5340BLE"
3. Connect to the device
4. Navigate to the custom service
5. Write data to the characteristic



## Expected Behavior

### On Your Computer
- Script should find "Dan5340BLE" device
- Should connect successfully
- Should write test data without errors

### Device Information
- **Device Name**: Dan5340BLE
- **Service UUID**: bc9a7856-3412-3412-3412-341278563412
- **Writable Characteristic UUID**: 21436587-a9cb-2143-2143-214321436587

### On nRF5340 Serial Console
You should see output like:
```
Received 17 bytes: 48 65 6c 6c 6f 20 66 72 6f 6d 20 50 79 74 68 6f 6e 
ASCII: Hello from Python!
```

## Troubleshooting

### Device Not Found
- Ensure nRF5340 is powered on
- Check that Bluetooth is enabled on your computer
- Verify the device is advertising (check serial console)
- Make sure you're within range

### Connection Failed
- Check if another device is already connected
- Try power cycling the nRF5340
- Verify the device name matches exactly

### Write Failed
- Check the characteristic UUID in the code
- Ensure the characteristic supports writing
- Verify the device is in the correct state

### Serial Console Issues
- Check baud rate (115200)
- Ensure correct USB port
- Try different serial terminal applications

## Testing Different Data

To test with different data, modify the `TEST_DATA` variable in `test_ble_python.py`:

```python
# Test with different data
TEST_DATA = b"Custom test message!"
TEST_DATA = b"12345"
TEST_DATA = b"Special chars: !@#$%^&*()"
```

## Debug Information

The Python script provides detailed debugging:
- Lists all discovered BLE devices
- Shows service and characteristic discovery
- Reports specific error messages
- Displays connection status

## Next Steps

Once BLE communication is working:
1. Test with larger data packets
2. Implement automatic reconnection
3. Add data validation
4. Move to WASM integration planning

## Common Issues on macOS

- **Permission denied**: Grant Bluetooth permissions in System Preferences
- **Bluetooth not available**: Check if Bluetooth is enabled
- **Device not found**: Try restarting Bluetooth service
- **Connection drops**: macOS may have aggressive power management
