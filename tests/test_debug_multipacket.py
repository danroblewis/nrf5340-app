#!/usr/bin/env python3
"""
Debug Multi-Packet WASM Upload

Compare working single packet vs failing multi-packet upload.
"""

import asyncio
import struct
import pytest
from pathlib import Path

# BLE UUIDs
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"


async def upload_debug_wasm(ble_client, upload_char, wasm_data, description):
    """Upload WASM with detailed debugging"""
    print(f"\n🔍 {description}")
    print(f"📦 WASM size: {len(wasm_data)} bytes")
    
    chunk_size = 244
    num_packets = (len(wasm_data) + chunk_size - 1) // chunk_size
    print(f"📊 Will need {num_packets} packets of max {chunk_size} bytes each")
    
    for offset in range(0, len(wasm_data), chunk_size):
        chunk = wasm_data[offset:offset + chunk_size]
        is_start = (offset == 0)
        
        if is_start:
            cmd = 0x01  # WASM_CMD_START_UPLOAD
            sequence = 0
            packet = struct.pack('<BBHI', cmd, sequence, len(chunk), len(wasm_data)) + chunk
            print(f"📤 START packet: cmd={cmd:02x}, seq={sequence}, chunk_len={len(chunk)}, total={len(wasm_data)}")
        else:
            cmd = 0x02  # WASM_CMD_CONTINUE_UPLOAD
            sequence = offset // chunk_size
            packet = struct.pack('<BBHI', cmd, sequence, len(chunk), len(wasm_data)) + chunk
            print(f"📤 CONTINUE packet: cmd={cmd:02x}, seq={sequence}, chunk_len={len(chunk)}, total={len(wasm_data)}")
        
        print(f"   🔢 Packet size: {len(packet)} bytes")
        print(f"   📝 First 16 bytes: {packet[:16].hex()}")
        print(f"   📝 Last 16 bytes: {packet[-16:].hex()}")
        
        await ble_client.write_gatt_char(upload_char, packet, response=False)
        print(f"   ✅ Packet {sequence} written")
        await asyncio.sleep(0.1)  # Delay between packets


async def check_status(ble_client, status_char, description):
    """Check WASM status"""
    status_data = await ble_client.read_gatt_char(status_char)
    if len(status_data) >= 4:
        status, error_code, bytes_received = struct.unpack('<BBH', status_data[:4])
        status_names = {0: "IDLE", 1: "RECEIVING", 2: "RECEIVED", 3: "LOADED", 4: "EXECUTING", 5: "COMPLETE", 6: "ERROR"}
        error_names = {0: "NONE", 1: "BUFFER_OVERFLOW", 2: "INVALID_MAGIC", 3: "LOAD_FAILED", 4: "COMPILE_FAILED", 5: "FUNCTION_NOT_FOUND", 6: "EXECUTION_FAILED", 7: "INVALID_PARAMS"}
        
        print(f"📊 Status {description}: {status_names.get(status, status)} (status={status}), Error: {error_names.get(error_code, error_code)} (error={error_code}), Bytes: {bytes_received}")
        return status, error_code, bytes_received
    else:
        print(f"⚠️ Status {description}: Unexpected data length {len(status_data)}")
        return None, None, None


@pytest.mark.asyncio
async def test_debug_multipacket_comparison(ble_client, ble_characteristics, serial_capture):
    """Compare working vs failing WASM uploads"""
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    status_char = ble_characteristics[WASM_STATUS_UUID]
    
    with serial_capture:
        # Test 1: Working 8-byte WASM
        print("🟢 === TEST 1: Working 8-byte WASM ===")
        
        # Reset
        reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)
        await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
        await asyncio.sleep(0.3)
        
        # Upload 8-byte WASM
        tiny_wasm = b'\x00\x61\x73\x6D\x01\x00\x00\x00'  # WASM magic + version
        await upload_debug_wasm(ble_client, upload_char, tiny_wasm, "Testing 8-byte WASM")
        await asyncio.sleep(1.0)
        
        status1, error1, bytes1 = await check_status(ble_client, status_char, "after 8-byte upload")
        
        print("\n" + "="*60)
        
        # Test 2: Failing multi-packet WASM
        print("🔴 === TEST 2: Failing multi-packet WASM ===")
        
        # Reset
        await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
        await asyncio.sleep(0.3)
        
        # Create a medium-sized WASM (32 bytes - will need 1 packet, header + 32 = 40 bytes)
        medium_wasm = b'\x00\x61\x73\x6D\x01\x00\x00\x00' + b'\x00' * 24  # 32 bytes total
        await upload_debug_wasm(ble_client, upload_char, medium_wasm, "Testing 32-byte WASM (single packet)")
        await asyncio.sleep(1.0)
        
        status2, error2, bytes2 = await check_status(ble_client, status_char, "after 32-byte upload")
        
        print("\n" + "="*60)
        
        # Test 3: Multi-packet WASM  
        print("🔴 === TEST 3: Multi-packet WASM ===")
        
        # Reset
        await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
        await asyncio.sleep(0.3)
        
        # Create WASM that requires 2 packets (300 bytes - will need 2 packets)
        large_wasm = b'\x00\x61\x73\x6D\x01\x00\x00\x00' + b'\x00' * 292  # 300 bytes total
        await upload_debug_wasm(ble_client, upload_char, large_wasm, "Testing 300-byte WASM (multi-packet)")
        await asyncio.sleep(1.0)
        
        status3, error3, bytes3 = await check_status(ble_client, status_char, "after 300-byte upload")
    
    # Check serial output
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    
    print(f"\n📋 Serial output captured ({len(serial_output)} chars):")
    if serial_output:
        print(serial_output[-2000:])  # Show last 2000 chars
    else:
        print("(No serial output captured)")
    
    print(f"\n🎯 SUMMARY:")
    print(f"   8-byte:   Status={status1}, Bytes={bytes1}")
    print(f"   32-byte:  Status={status2}, Bytes={bytes2}")  
    print(f"   300-byte: Status={status3}, Bytes={bytes3}")
    
    if status1 == 3 and status2 == 3 and status3 != 3:
        print("🔍 Issue is specifically with multi-packet uploads!")
    elif status1 == 3 and status2 != 3:
        print("🔍 Issue appears around 32-byte mark!")
    elif status1 != 3:
        print("🔍 Issue is with WASM uploads in general!")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
