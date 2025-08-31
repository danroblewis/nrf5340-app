#!/usr/bin/env python3
"""
Debug WASM Handler Test

This test monitors serial output to see what happens when we send WASM upload packets.
"""

import asyncio
import struct
import pytest
from pathlib import Path

# BLE UUIDs
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"


@pytest.mark.asyncio
async def test_debug_wasm_handler_calls(ble_client, ble_characteristics, serial_capture):
    """Test to debug if WASM upload handler is being called"""
    
    print("\n🔍 Testing WASM upload handler calls...")
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    status_char = ble_characteristics[WASM_STATUS_UUID]
    
    with serial_capture:
        print("📡 Step 1: Sending reset command...")
        reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)  # WASM_CMD_RESET
        await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
        await asyncio.sleep(0.5)
        
        print("📡 Step 2: Sending tiny WASM upload packet...")
        # Send minimal WASM magic + version (8 bytes)
        tiny_wasm = b'\x00\x61\x73\x6D\x01\x00\x00\x00'
        cmd = 0x01  # WASM_CMD_START_UPLOAD
        sequence = 0
        chunk_len = len(tiny_wasm)
        total_len = len(tiny_wasm)
        
        packet = struct.pack('<BBHI', cmd, sequence, chunk_len, total_len) + tiny_wasm
        print(f"📤 Sending packet: {len(packet)} bytes")
        print(f"   Header: cmd={cmd:02x}, seq={sequence}, chunk_len={chunk_len}, total_len={total_len}")
        print(f"   Full packet hex: {packet.hex()}")
        
        await ble_client.write_gatt_char(upload_char, packet, response=False)
        await asyncio.sleep(1.0)
        
        print("📡 Step 3: Reading status...")
        status_data = await ble_client.read_gatt_char(status_char)
        if len(status_data) >= 4:
            status, error_code, bytes_received = struct.unpack('<BBH', status_data[:4])
            print(f"📊 Final Status: {status}, Error: {error_code}, Bytes: {bytes_received}")
        else:
            print(f"⚠️ Unexpected status data: {status_data.hex()}")
    
    # Check serial output
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    
    print(f"\n📋 Serial output captured ({len(serial_output)} chars):")
    if serial_output:
        print(serial_output)
    else:
        print("(No serial output captured)")
        
    # Look for specific debug messages
    if "=== WASM Service: wasm_upload_handler called ===" in serial_output:
        print("✅ WASM upload handler WAS called!")
    else:
        print("❌ WASM upload handler was NOT called!")
        
    if "WASM Service: Upload packet received" in serial_output:
        print("✅ Upload packet was processed!")
    else:
        print("❌ Upload packet was NOT processed!")
        
    if "Packet too large" in serial_output:
        print("⚠️ Found 'Packet too large' error in serial output!")
        
    if "wasm_upload_handler" in serial_output:
        print("✅ Found wasm_upload_handler mentions in serial output!")
    else:
        print("❌ No wasm_upload_handler mentions found!")
    
    # Don't assert anything - just collect debug info
    print("\n🎯 Debug test complete - check output above for clues!")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
