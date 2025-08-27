#!/usr/bin/env python3
"""
Test script for wasm3 integration on nRF5340
Sends a minimal WASM binary via BLE to test the wasm3 wrapper
"""

import asyncio
import struct
from bleak import BleakScanner, BleakClient

# Device name to look for
DEVICE_NAME = "Dan5340BLE"

# Service UUID discovered from debug output (byte-reversed from our code)
CUSTOM_SERVICE_UUID = "bc9a7856-3412-3412-3412-341278563412"

# Minimal WASM binary (just the magic number and version for testing)
# This is a minimal valid WASM module that should trigger our wasm3 wrapper
MINIMAL_WASM = bytes([
    0x00, 0x61, 0x73, 0x6d,  # WASM magic number
    0x01, 0x00, 0x00, 0x00,  # Version 1
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
    """Test the wasm3 integration by sending a minimal WASM binary"""
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
        
        # Send the minimal WASM binary
        print(f"Sending minimal WASM binary ({len(MINIMAL_WASM)} bytes)...")
        print(f"WASM bytes: {' '.join(f'{b:02x}' for b in MINIMAL_WASM)}")
        
        await client.write_gatt_char(writable_char.uuid, MINIMAL_WASM)
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
