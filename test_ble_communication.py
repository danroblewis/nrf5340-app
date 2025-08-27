#!/usr/bin/env python3
"""
Simple BLE Test for nRF5340
This script tests basic BLE communication
"""

import asyncio
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# Configuration
DEVICE_NAME = "Dan5340BLE"
# Actual UUID discovered from the device
CHAR_UUID = "21436587-a9cb-2143-2143-214321436587"

async def main():
    """Main test function."""
    print("🚀 Simple BLE Test for nRF5340")
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
            
            # Try to write data
            test_data = b"Hello from Python!"
            print(f"\n✍️  Writing: '{test_data.decode()}'")
            print(f"   Hex: {test_data.hex()}")
            
            try:
                await client.write_gatt_char(CHAR_UUID, test_data)
                print("✅ Data written successfully!")
                print("\n🎉 Test completed! Check your nRF5340 serial console.")
                
            except BleakError as e:
                print(f"❌ Write failed: {e}")
                print("\n💡 This might be because:")
                print("   - The characteristic UUID is incorrect")
                print("   - The characteristic doesn't support writing")
                print("   - The device needs to be in a specific state")
                
                # Try to get more info about what's available
                print("\n🔍 Trying to get device info...")
                try:
                    # Try a different approach to get services
                    print("   Note: Service discovery may not work with this bleak version")
                    print("   You may need to use nRF Connect app to see the actual services")
                except Exception as e2:
                    print(f"   Error getting device info: {e2}")
                
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
