#!/usr/bin/env python3
"""
Debug WASM State Tracking

Track the exact state after each packet to see where the state changes unexpectedly.
"""

import asyncio
import struct
import pytest

# BLE UUIDs
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"


async def check_detailed_status(ble_client, status_char, description):
    """Check WASM status with detailed logging"""
    status_data = await ble_client.read_gatt_char(status_char)
    if len(status_data) >= 4:
        status, error_code, bytes_received = struct.unpack('<BBH', status_data[:4])
        status_names = {0: "IDLE", 1: "RECEIVING", 2: "RECEIVED", 3: "LOADED", 4: "EXECUTING", 5: "COMPLETE", 6: "ERROR"}
        error_names = {0: "NONE", 1: "BUFFER_OVERFLOW", 2: "INVALID_MAGIC", 3: "LOAD_FAILED", 4: "COMPILE_FAILED", 5: "FUNCTION_NOT_FOUND", 6: "EXECUTION_FAILED", 7: "INVALID_PARAMS"}
        
        print(f"📊 {description}:")
        print(f"   Status: {status} ({status_names.get(status, 'UNKNOWN')})")
        print(f"   Error:  {error_code} ({error_names.get(error_code, 'UNKNOWN')})")
        print(f"   Bytes:  {bytes_received}")
        
        return status, error_code, bytes_received
    else:
        print(f"⚠️ {description}: Unexpected data length {len(status_data)}")
        return None, None, None


@pytest.mark.asyncio
async def test_debug_state_after_each_packet(ble_client, ble_characteristics, serial_capture):
    """Debug state transitions during multi-packet upload"""
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    status_char = ble_characteristics[WASM_STATUS_UUID]
    
    with serial_capture:
        print("\n🔍 Testing state tracking during multi-packet upload...")
        
        # Reset
        print("📡 Reset...")
        reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)
        await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
        await asyncio.sleep(0.3)
        
        await check_detailed_status(ble_client, status_char, "After reset")
        
        # Create 300-byte WASM for 2-packet test
        wasm_data = b'\x00\x61\x73\x6D\x01\x00\x00\x00' + b'\x00' * 292  # 300 bytes total
        chunk_size = 244
        
        print(f"\n📦 Uploading {len(wasm_data)} bytes in {(len(wasm_data) + chunk_size - 1) // chunk_size} packets...")
        
        # Packet 1: START
        chunk1 = wasm_data[0:244]
        packet1 = struct.pack('<BBHI', 0x01, 0, len(chunk1), len(wasm_data)) + chunk1
        
        print(f"\n📤 Sending packet 1:")
        print(f"   Command: 0x01 (START)")
        print(f"   Sequence: 0")
        print(f"   Chunk size: {len(chunk1)}")
        print(f"   Total size: {len(wasm_data)}")
        print(f"   Packet size: {len(packet1)} bytes")
        
        await ble_client.write_gatt_char(upload_char, packet1, response=False)
        await asyncio.sleep(0.1)  # Short delay
        
        status1, error1, bytes1 = await check_detailed_status(ble_client, status_char, "After packet 1")
        
        if status1 != 1:  # Not RECEIVING
            print(f"❌ Expected RECEIVING (1) after packet 1, got {status1}")
            print("Continuing to check serial output for clues...")
        else:
            print("✅ Packet 1 processed correctly, continuing to packet 2...")
        
        # Small delay before packet 2
        print(f"\n⏱️ Waiting 100ms before packet 2...")
        await asyncio.sleep(0.1)
        
        # Check status again right before packet 2
        status_pre2, error_pre2, bytes_pre2 = await check_detailed_status(ble_client, status_char, "Right before packet 2")
        
        if status_pre2 != 1:  # Not RECEIVING
            print(f"❌ State changed from RECEIVING to {status_pre2} before packet 2!")
            print("This indicates a timing issue or background processing changed the state")
        
        # Packet 2: CONTINUE
        chunk2 = wasm_data[244:300]
        packet2 = struct.pack('<BBHI', 0x02, 1, len(chunk2), len(wasm_data)) + chunk2
        
        print(f"\n📤 Sending packet 2:")
        print(f"   Command: 0x02 (CONTINUE)")
        print(f"   Sequence: 1")
        print(f"   Chunk size: {len(chunk2)}")
        print(f"   Total size: {len(wasm_data)}")
        print(f"   Packet size: {len(packet2)} bytes")
        
        await ble_client.write_gatt_char(upload_char, packet2, response=False)
        await asyncio.sleep(0.1)
        
        status2, error2, bytes2 = await check_detailed_status(ble_client, status_char, "After packet 2")
        
        print(f"\n🎯 ANALYSIS:")
        print(f"   Expected after packet 1: RECEIVING (1), got {status1}")
        print(f"   Expected after packet 2: RECEIVED (2), got {status2}")
        
        if status1 == 1 and status2 != 2:
            print(f"   Issue: Packet 2 processing failed")
        elif status1 != 1:
            print(f"   Issue: Packet 1 didn't set correct state")
    
    # Serial output
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    
    print(f"\n📋 Serial output:")
    if serial_output:
        lines = serial_output.split('\n')
        for i, line in enumerate(lines):
            if line.strip():
                print(f"   {i+1:2d}: {line}")
    else:
        print("   (No serial output captured)")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
