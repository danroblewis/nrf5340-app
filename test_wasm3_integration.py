#!/usr/bin/env python3
"""
Test script for wasm3 integration on nRF5340
Sends a proper WASM binary via BLE to test the wasm3 wrapper
"""

import asyncio
import struct
from bleak import BleakScanner, BleakClient

# Device name to look for
DEVICE_NAME = "Dan5340BLE"

# Service UUID discovered from debug output (byte-reversed from our code)
CUSTOM_SERVICE_UUID = "bc9a7856-3412-3412-3412-341278563412"

# A simple WASM module that exports a function called "add"
# This is a minimal but complete WASM module that adds two i32 values
SIMPLE_WASM = bytes([
    # WASM magic number and version
    0x00, 0x61, 0x73, 0x6d,  # WASM magic number
    0x01, 0x00, 0x00, 0x00,  # Version 1
    
    # Type section (1)
    0x01, 0x07, 0x01,        # Section 1, length 7, 1 type
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,  # func (i32, i32) -> i32
    
    # Function section (3)
    0x03, 0x02, 0x01, 0x00,  # Section 3, length 2, 1 function, type index 0
    
    # Export section (7)
    0x07, 0x0a, 0x01,        # Section 7, length 10, 1 export
    0x03, 0x61, 0x64, 0x64,  # String "add" (length 3)
    0x00, 0x00,              # Export kind 0 (function), function index 0
    
    # Code section (10)
    0x0a, 0x04, 0x01,        # Section 10, length 4, 1 function
    0x02, 0x00,              # Function body size 2, 0 locals
    0x20, 0x00,              # local.get 0 (get first argument)
    0x20, 0x01,              # local.get 1 (get second argument)
    0x6a,                    # i32.add
    0x0b,                    # end
])

async def find_device():
    """Find our BLE device by name"""
    print(f"Scanning for device: {DEVICE_NAME}")
    
    devices = await BleakScanner.discover(timeout=10.0)
    
    for device in devices:
        if device.name == DEVICE_NAME:
            print(f"Found device: {device.name} ({device.address})")
            return device
    
    print("Device not found!")
    return None

async def test_wasm3_integration():
    """Test the wasm3 integration by sending a proper WASM binary"""
    device = await find_device()
    if not device:
        return
    
    print(f"Connecting to {device.name}...")
    
    async with BleakClient(device.address) as client:
        print("Connected!")
        
        # Discover services
        print("Discovering services...")
        services = client.services
        
        print("Available services:")
        for service in services:
            print(f"  Service: {service.uuid}")
            for char in service.characteristics:
                print(f"    Characteristic: {char.uuid} - Properties: {char.properties}")
        
        # Look for our custom service using the discovered UUID
        custom_service = None
        for service in services:
            if service.uuid == CUSTOM_SERVICE_UUID:
                custom_service = service
                break
        
        if not custom_service:
            print(f"Custom service {CUSTOM_SERVICE_UUID} not found!")
            print("Available service UUIDs:")
            for service in services:
                print(f"  {service.uuid}")
            return
        
        print(f"Found custom service: {custom_service.uuid}")
        
        # Look for the writable characteristic
        writable_char = None
        for char in custom_service.characteristics:
            if "write" in char.properties:
                writable_char = char
                break
        
        if not writable_char:
            print("Writable characteristic not found!")
            return
        
        print(f"Found writable characteristic: {writable_char.uuid}")
        
        # Send the proper WASM binary
        print(f"Sending WASM binary ({len(SIMPLE_WASM)} bytes)...")
        print(f"WASM bytes: {' '.join(f'{b:02x}' for b in SIMPLE_WASM)}")
        print("This WASM module exports a function called 'add' that adds two i32 values")
        
        await client.write_gatt_char(writable_char.uuid, SIMPLE_WASM)
        print("WASM binary sent successfully!")
        
        # Wait a bit for processing
        await asyncio.sleep(2)
        print("Test completed!")

if __name__ == "__main__":
    print("Testing wasm3 integration on nRF5340")
    print("=" * 50)
    
    try:
        asyncio.run(test_wasm3_integration())
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Test failed: {e}")
