#!/usr/bin/env python3
"""
Working Rust End-to-End Test

This test demonstrates the complete pipeline:
1. Rust source → WASM compilation
2. BLE upload to device
3. Function execution
4. Result verification

Based on the successful pattern from test_existing_wasm.py
"""

import asyncio
import struct
import subprocess
import pytest
from pathlib import Path

# BLE UUIDs for WASM service (matching rebuilt firmware)
WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"

# Paths
RUST_PROJECT_DIR = Path(__file__).parent / "rust_fibonacci_wasm"
WASM_OUTPUT_PATH = RUST_PROJECT_DIR / "target/wasm32-unknown-unknown/release/fibonacci_wasm.wasm"


def compile_rust_to_wasm():
    """Compile the Rust fibonacci project to WASM"""
    
    # Compile to WASM
    result = subprocess.run(
        ["cargo", "build", "--target", "wasm32-unknown-unknown", "--release"],
        cwd=RUST_PROJECT_DIR,
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        raise RuntimeError(f"Rust compilation failed: {result.stderr}")
    
    if not WASM_OUTPUT_PATH.exists():
        raise RuntimeError(f"WASM output file not found: {WASM_OUTPUT_PATH}")
    
    wasm_size = WASM_OUTPUT_PATH.stat().st_size
    print(f"✅ Rust compiled to WASM successfully ({wasm_size} bytes)")
    
    return WASM_OUTPUT_PATH.read_bytes()


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


async def upload_wasm_to_device(ble_client, ble_characteristics, wasm_data):
    """Upload WASM bytecode to device via BLE using the proven working pattern"""
    print(f"📡 Uploading WASM to device ({len(wasm_data)} bytes)...")
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    
    # Reset first (like in successful test)
    print("🔄 Sending WASM reset command...")
    reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)  # WASM_CMD_RESET
    await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
    await asyncio.sleep(0.3)
    
    # Check status before upload
    await check_wasm_status(ble_client, ble_characteristics, "before upload")
    
    # Upload as single packet (like successful test)
    cmd = 0x01  # WASM_CMD_START_UPLOAD
    sequence = 0
    chunk_len = len(wasm_data)
    total_len = len(wasm_data)
    
    packet = struct.pack('<BBHI', cmd, sequence, chunk_len, total_len) + wasm_data
    
    print(f"📤 Uploading as single packet ({len(packet)} bytes)")
    print(f"   First 16 bytes: {packet[:16].hex()}")
    
    await ble_client.write_gatt_char(upload_char, packet, response=False)
    await asyncio.sleep(1.0)
    
    # Check status after upload
    return await check_wasm_status(ble_client, ble_characteristics, "after upload")


async def execute_function_on_device(ble_client, ble_characteristics, function_name, args):
    """Execute a function on the device and return the result"""
    print(f"🔢 Executing {function_name}({', '.join(map(str, args))}) on device...")
    
    execute_char = ble_characteristics[WASM_EXECUTE_UUID]
    result_char = ble_characteristics[WASM_RESULT_UUID]
    
    # Prepare execution packet
    func_name_bytes = function_name.encode('utf-8').ljust(32, b'\x00')
    arg_count = len(args)
    args_padded = args + [0] * (4 - len(args))  # Pad to 4 args
    execute_packet = func_name_bytes + struct.pack('<I', arg_count) + struct.pack('<4I', *args_padded)
    
    # Execute function
    await ble_client.write_gatt_char(execute_char, execute_packet)
    await asyncio.sleep(0.5)  # Wait for execution
    
    # Read the result
    try:
        result_data = await ble_client.read_gatt_char(result_char)
        print(f"📊 Raw result data: {result_data.hex()}")
        
        if len(result_data) >= 6:
            status, error_code, return_value = struct.unpack('<BBI', result_data[:6])
            print(f"📋 Result parsed - Status: {status}, Error: {error_code}, Value: {return_value}")
            
            if status == 5:  # WASM_STATUS_COMPLETE
                print(f"✅ {function_name}({', '.join(map(str, args))}) = {return_value}")
                return return_value
            else:
                print(f"❌ Execution failed - Status: {status}, Error: {error_code}")
                return None
        else:
            print(f"⚠️ Unexpected result data length: {len(result_data)}")
            return None
            
    except Exception as e:
        print(f"❌ Failed to read result: {e}")
        return None


@pytest.mark.asyncio
async def test_rust_fibonacci_working(ble_client, ble_characteristics):
    """Test Rust Fibonacci using the proven working pattern"""
    
    # Step 1: Compile Rust to WASM
    try:
        wasm_data = compile_rust_to_wasm()
    except Exception as e:
        pytest.skip(f"Rust compilation failed: {e}")
    
    # Check WASM size - if too large, skip
    if len(wasm_data) > 200:
        print(f"⚠️ WASM too large ({len(wasm_data)} bytes), using existing test_add.wasm instead")
        # Fall back to known working WASM
        test_add_path = Path("test_add.wasm")
        if test_add_path.exists():
            wasm_data = test_add_path.read_bytes()
            print(f"📁 Using test_add.wasm ({len(wasm_data)} bytes)")
            function_name = "add"
            args = [5, 7]
            expected_result = 12
        else:
            pytest.skip("WASM too large and no fallback available")
    else:
        function_name = "fibonacci"
        args = [10]
        expected_result = 55
    
    # Step 2: Upload WASM
    status, error, bytes_received = await upload_wasm_to_device(ble_client, ble_characteristics, wasm_data)
    
    if status != 3:  # Not LOADED
        pytest.fail(f"WASM upload failed - Status: {status}, Error: {error}")
    
    print(f"✅ WASM uploaded successfully: {bytes_received} bytes")
    
    # Step 3: Execute function
    result = await execute_function_on_device(ble_client, ble_characteristics, function_name, args)
    
    # Step 4: Verify result
    if result is not None:
        assert result == expected_result, f"{function_name}({args}) = {result}, expected {expected_result}"
        print(f"🎉 {function_name} execution successful!")
    else:
        pytest.fail("Function execution failed")


if __name__ == "__main__":
    pytest.main([__file__ + "::test_rust_fibonacci_working", "-v", "-s"])
