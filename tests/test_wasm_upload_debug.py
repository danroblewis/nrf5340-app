#!/usr/bin/env python3
"""
Debug WASM Upload - Simplified test to debug upload issues
"""

import pytest
import asyncio
import struct
from pathlib import Path

# WASM Service UUIDs (matching rebuilt firmware)
WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"

# Simple test WASM data (just magic + minimal structure)
TEST_WASM = bytes([
    0x00, 0x61, 0x73, 0x6d,  # WASM magic
    0x01, 0x00, 0x00, 0x00,  # WASM version
    # Minimal empty WASM module
])


async def check_wasm_status(ble_client, ble_characteristics, description=""):
    """Check and print WASM service status"""
    status_char = ble_characteristics[WASM_STATUS_UUID]
    status_data = await ble_client.read_gatt_char(status_char)
    
    if len(status_data) >= 4:
        status, error_code, bytes_received = struct.unpack('<BBH', status_data[:4])
        status_names = {0: "IDLE", 1: "RECEIVING", 2: "RECEIVED", 3: "LOADED", 4: "EXECUTING", 5: "COMPLETE", 6: "ERROR"}
        error_names = {0: "NONE", 1: "BUFFER_OVERFLOW", 2: "INVALID_MAGIC", 3: "LOAD_FAILED", 4: "COMPILE_FAILED", 5: "FUNCTION_NOT_FOUND", 6: "EXECUTION_FAILED", 7: "INVALID_PARAMS"}
        
        print(f"📊 WASM Status {description}: {status_names.get(status, status)} (status={status}), Error: {error_names.get(error_code, error_code)} (error={error_code}), Bytes: {bytes_received}")
        return status, error_code, bytes_received
    else:
        print(f"⚠️ WASM Status {description}: Unexpected data length {len(status_data)}")
        return None, None, None


@pytest.mark.asyncio
async def test_simple_wasm_upload(ble_client, ble_characteristics):
    """Test simple WASM upload to debug the upload process"""
    
    print(f"\n🧪 Testing simple WASM upload ({len(TEST_WASM)} bytes)")
    
    # Check initial status
    status, error, bytes_received = await check_wasm_status(ble_client, ble_characteristics, "initial")
    assert status == 0, f"Expected IDLE status initially, got {status}"
    
    # Get upload characteristic
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    
    # Send simple upload packet (all data in one packet)
    cmd = 0x01  # WASM_CMD_START_UPLOAD
    sequence = 0
    chunk_len = len(TEST_WASM)
    total_len = len(TEST_WASM)
    
    packet = struct.pack('<BBHI', cmd, sequence, chunk_len, total_len) + TEST_WASM
    
    print(f"📤 Sending single upload packet:")
    print(f"   cmd={cmd:02x}, seq={sequence}, chunk_len={chunk_len}, total_len={total_len}")
    print(f"   packet_size={len(packet)} bytes")
    print(f"   packet_hex={packet.hex()}")
    
    # Write the packet
    await ble_client.write_gatt_char(upload_char, packet, response=False)
    print("✅ Packet written successfully")
    
    # Wait for processing
    await asyncio.sleep(1.0)
    
    # Check status after upload
    status, error, bytes_received = await check_wasm_status(ble_client, ble_characteristics, "after upload")
    
    # Analyze results
    if status == 0:  # Still IDLE
        print("❌ Upload failed - status still IDLE, bytes not received")
        print("🔍 Possible issues:")
        print("   - Firmware not processing upload characteristic")
        print("   - Wrong characteristic UUID")
        print("   - Firmware bug in upload handler")
    elif status == 1:  # RECEIVING
        print("⚠️ Upload partially working - device is receiving but not complete")
    elif status == 2:  # RECEIVED
        print("✅ Upload received but not loaded/compiled")
    elif status == 3:  # LOADED
        print("🎉 Upload successful - WASM loaded!")
    elif status == 6:  # ERROR
        print(f"❌ Upload error - error code {error}")
    
    # Assert some progress was made
    assert status != 0 or bytes_received > 0, "Upload made no progress - firmware may not be processing uploads"


@pytest.mark.asyncio 
async def test_wasm_reset_command(ble_client, ble_characteristics):
    """Test sending a WASM reset command to clear state"""
    
    print(f"\n🔄 Testing WASM reset command")
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    
    # Send reset command
    cmd = 0x04  # WASM_CMD_RESET
    sequence = 0
    chunk_len = 0
    total_len = 0
    
    packet = struct.pack('<BBHI', cmd, sequence, chunk_len, total_len)
    
    print(f"📤 Sending reset packet: {packet.hex()}")
    
    await ble_client.write_gatt_char(upload_char, packet, response=False)
    await asyncio.sleep(0.5)
    
    # Check status after reset
    await check_wasm_status(ble_client, ble_characteristics, "after reset")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
