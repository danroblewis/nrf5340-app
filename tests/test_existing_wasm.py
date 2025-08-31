#!/usr/bin/env python3
"""
Test using existing WASM files to verify upload mechanism
"""

import pytest
import asyncio
import struct
from pathlib import Path

# WASM Service UUIDs
WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"

# Test WASM files
TEST_ADD_WASM = Path("test_add.wasm")
TEST_WASM = Path("test.wasm")


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


async def upload_wasm_file(ble_client, ble_characteristics, wasm_file_path):
    """Upload a WASM file to the device"""
    
    wasm_data = wasm_file_path.read_bytes()
    print(f"📁 Loading WASM file: {wasm_file_path} ({len(wasm_data)} bytes)")
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    
    # Reset first
    print("🔄 Sending WASM reset command...")
    reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)
    await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
    await asyncio.sleep(0.3)
    
    # Check status before upload
    await check_wasm_status(ble_client, ble_characteristics, "before upload")
    
    # Upload as single packet
    cmd = 0x01  # WASM_CMD_START_UPLOAD  
    sequence = 0
    chunk_len = len(wasm_data)
    total_len = len(wasm_data)
    
    packet = struct.pack('<BBHI', cmd, sequence, chunk_len, total_len) + wasm_data
    
    print(f"📤 Uploading {wasm_file_path.name} as single packet ({len(packet)} bytes)")
    print(f"   First 16 bytes: {packet[:16].hex()}")
    
    await ble_client.write_gatt_char(upload_char, packet, response=False)
    await asyncio.sleep(1.0)
    
    # Check status after upload
    return await check_wasm_status(ble_client, ble_characteristics, "after upload")


@pytest.mark.asyncio
async def test_existing_add_wasm(ble_client, ble_characteristics):
    """Test uploading the existing test_add.wasm file"""
    
    if not TEST_ADD_WASM.exists():
        pytest.skip(f"Test file {TEST_ADD_WASM} not found")
    
    print(f"\n🧪 Testing existing add WASM: {TEST_ADD_WASM}")
    
    status, error, bytes_received = await upload_wasm_file(ble_client, ble_characteristics, TEST_ADD_WASM)
    
    print(f"📊 Upload result: Status={status}, Error={error}, Bytes={bytes_received}")
    
    # Check if upload was successful
    if status == 3:  # LOADED
        print("✅ WASM loaded successfully!")
        
        # Try to execute the add function
        execute_char = ble_characteristics[WASM_EXECUTE_UUID]
        result_char = ble_characteristics[WASM_RESULT_UUID]
        
        # Prepare execution packet for add(5, 7)
        function_name = b"add".ljust(32, b'\x00')
        arg_count = 2
        args = [5, 7, 0, 0]
        execute_packet = function_name + struct.pack('<I', arg_count) + struct.pack('<4I', *args)
        
        print("🔢 Executing add(5, 7)...")
        await ble_client.write_gatt_char(execute_char, execute_packet)
        await asyncio.sleep(0.5)
        
        # Read result
        result_data = await ble_client.read_gatt_char(result_char)
        if len(result_data) >= 6:
            status, error_code, return_value = struct.unpack('<BBI', result_data[:6])
            print(f"📋 Execution result: Status={status}, Error={error_code}, Value={return_value}")
            
            if status == 5:  # COMPLETE
                assert return_value == 12, f"Expected add(5,7)=12, got {return_value}"
                print("🎉 Add function executed successfully!")
            else:
                print(f"❌ Execution failed with status {status}, error {error_code}")
        
    elif status == 6:  # ERROR
        print(f"❌ Upload failed with error code {error}")
    else:
        print(f"⚠️ Unexpected status {status}")


@pytest.mark.asyncio
async def test_existing_test_wasm(ble_client, ble_characteristics):
    """Test uploading the existing test.wasm file"""
    
    if not TEST_WASM.exists():
        pytest.skip(f"Test file {TEST_WASM} not found")
    
    print(f"\n🧪 Testing existing test WASM: {TEST_WASM}")
    
    status, error, bytes_received = await upload_wasm_file(ble_client, ble_characteristics, TEST_WASM)
    
    print(f"📊 Upload result: Status={status}, Error={error}, Bytes={bytes_received}")
    
    if status == 3:  # LOADED
        print("✅ Test WASM loaded successfully!")
    elif status == 6:  # ERROR  
        print(f"❌ Upload failed with error code {error}")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
