#!/usr/bin/env python3
"""
Test sending WASM binary to nRF5340
This script sends a minimal WASM module to test WAMR integration
"""

import asyncio
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# Configuration
DEVICE_NAME = "Dan5340BLE"
CHAR_UUID = "21436587-a9cb-2143-2143-214321436587"

# Minimal WASM module (version 1, empty sections)
# This is the same as our test_wasm_module.h
MINIMAL_WASM = bytes([
    0x00, 0x61, 0x73, 0x6d,  # Magic number: "\0asm"
    0x01, 0x00, 0x00, 0x00,  # Version: 1
    0x00,                      # Custom section count
    0x00,                      # Type section count
    0x00,                      # Import section count
    0x00,                      # Function section count
    0x00,                      # Table section count
    0x00,                      # Memory section count
    0x00,                      # Global section count
    0x00,                      # Export section count
    0x00,                      # Start section count
    0x00,                      # Element section count
    0x00,                      # Code section count
    0x00                       # Data section count
])

async def main():
    """Main test function."""
    print("🚀 WASM Binary Test for nRF5340")
    print("=" * 40)
    
    # Scan for device
    print("🔍 Scanning for BLE devices...")
    devices = await BleakScanner.discover(timeout=10.0)
    
    target_device = None
    for device in devices:
        if device.name == DEVICE_NAME:
            target_device = device
            print(f"✅ Found {DEVICE_NAME} at {device.address}")
            break
    
    if not target_device:
        print(f"❌ Device '{DEVICE_NAME}' not found!")
        return
    
    # Connect to device
    print(f"\n🔗 Connecting to {target_device.address}...")
    try:
        async with BleakClient(target_device.address) as client:
            print("✅ Connected successfully!")
            
            # Send minimal WASM binary
            print(f"\n📦 Sending minimal WASM module ({len(MINIMAL_WASM)} bytes)")
            print(f"   Magic: {MINIMAL_WASM[:4].hex()}")
            print(f"   Version: {int.from_bytes(MINIMAL_WASM[4:8], 'little')}")
            
            try:
                await client.write_gatt_char(CHAR_UUID, MINIMAL_WASM)
                print("✅ WASM binary sent successfully!")
                print("\n🎉 Test completed! Check your nRF5340 serial console.")
                print("   The device should detect this as valid WASM and try to execute it.")
                
            except BleakError as e:
                print(f"❌ Write failed: {e}")
                
    except BleakError as e:
        print(f"❌ Connection failed: {e}")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n⏹️  Test interrupted by user")
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
