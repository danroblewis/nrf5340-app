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

# Service UUID from C code (exact byte order, not byte-reversed)
CUSTOM_SERVICE_UUID = "bc9a7856-3412-3412-3412-341278563412"

# Characteristic UUID from C code (exact byte order, not byte-reversed)  
CUSTOM_CHAR_UUID = "21436587-a9cb-2143-2143-214321436587"



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
    devices = await BleakScanner.discover(timeout=10.0)
    for device in devices:
        if device.name == DEVICE_NAME:
            return device
    print("Device not found!")
    return None


async def test_wasm3_integration():
    device = await find_device()
    if not device:
        return
    
    async with BleakClient(device.address) as client:
        services = client.services
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

        print(f"Found custom service: {custom_service.uuid}")
        
        # Look for the writable characteristic using the specific UUID
        writable_char = custom_service.characteristics[0]

        for attempt in range(11, 80, 3):
            # Wait 5 seconds before next attempt (except for the last one)
            await asyncio.sleep(2)
            
            print(f"\n--- Attempt {attempt}/10 ---")
            

            # # Try to use write-without-response first
            # try:
            #     resp = await client.write_gatt_char(writable_char.uuid, b"asdfasfasfd", response=False)
            #     print(f"Response: {resp}")
            #     print(f"WASM binary sent successfully asdfasfasfd with write-without-response! (Attempt {attempt}/10)")
            # except Exception as e:
            #     print("Write failed: {e}")
            
            # await asyncio.sleep(2)

            # Try to use write-without-response first
            try:
                # data_to_send = SIMPLE_WASM[:19]
                data_to_send = SIMPLE_WASM[:attempt]
                resp = await client.write_gatt_char(writable_char.uuid, data_to_send)
                print(f"Response: {resp}")
                print(f"WASM binary sent successfully with write-without-response! Sent first {attempt} bytes. (Attempt {attempt}/10)")
            except Exception as e:
                print(f"Write failed: {e}")
            
        
        print("\nAll 10 attempts completed!")

if __name__ == "__main__":
    try:
        asyncio.run(test_wasm3_integration())
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Test failed: {e}")
