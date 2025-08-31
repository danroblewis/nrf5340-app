#!/usr/bin/env python3
"""
End-to-end test for Rust Fibonacci WASM compilation and device execution.

This test demonstrates the complete pipeline:
1. Compile Rust source code to WASM
2. Upload WASM to nRF5340 device via BLE
3. Execute fibonacci function on device 
4. Verify correct results

This validates that arbitrary user code can be compiled and run on the device.
"""

import asyncio
import struct
import subprocess
import pytest
from pathlib import Path

# BLE UUIDs for WASM service (matching rebuilt firmware source code)
WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"  # Source: Upload = fff6
WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"  # Source: Result = fff3

# Paths
RUST_PROJECT_DIR = Path(__file__).parent / "rust_fibonacci_wasm"
WASM_OUTPUT_PATH = RUST_PROJECT_DIR / "target/wasm32-unknown-unknown/release/fibonacci_wasm.wasm"

# Test parameters: (input, expected_fibonacci_result)
FIBONACCI_TEST_CASES = [
    (0, 0),
    (1, 1),
    (2, 1),
    (3, 2),
    (4, 3),
    (5, 5),
    (6, 8),
    (7, 13),
    (8, 21),
    (9, 34),
    (10, 55),
    (15, 610),
]


def compile_rust_to_wasm():
    """Compile the Rust fibonacci project to WASM"""
    
    # Compile to WASM (assumes wasm32-unknown-unknown target is already installed)
    result = subprocess.run(
        ["cargo", "build", "--target", "wasm32-unknown-unknown", "--release"],
        cwd=RUST_PROJECT_DIR,
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        if "wasm32-unknown-unknown" in result.stderr:
            raise RuntimeError(f"WASM target not installed. Please run: rustup target add wasm32-unknown-unknown")
        raise RuntimeError(f"Rust compilation failed: {result.stderr}")
    
    if not WASM_OUTPUT_PATH.exists():
        raise RuntimeError(f"WASM file not found at: {WASM_OUTPUT_PATH}")
    
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
    """Upload WASM bytecode to device via BLE"""
    print(f"📡 Uploading WASM to device ({len(wasm_data)} bytes)...")
    
    # Get upload characteristic from our fixtures
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    
    # First, reset WASM state to ensure clean upload
    print("🔄 Sending WASM reset command...")
    reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)  # WASM_CMD_RESET
    await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
    await asyncio.sleep(0.2)
    
    # Check status before upload
    await check_wasm_status(ble_client, ble_characteristics, "before upload")
    
    # Upload in chunks as designed (respecting MTU limits)
    chunk_size = 244  # MTU - headers (as per firmware design)
    
    print(f"📦 Uploading {len(wasm_data)} bytes in chunks of {chunk_size}")
    
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
        
        print(f"🔢 Packet size: {len(packet)} bytes, first 16 bytes: {packet[:16].hex()}")
        
        try:
            await ble_client.write_gatt_char(upload_char, packet, response=False)  # Use write-without-response
            print(f"✅ Packet {sequence} written successfully")
        except Exception as e:
            print(f"❌ Packet {sequence} write failed: {e}")
            
        await asyncio.sleep(0.05)  # Longer delay between chunks for reliability
    
    await asyncio.sleep(1.0)  # Longer wait for upload to complete and process
    
    # Check status after upload
    return await check_wasm_status(ble_client, ble_characteristics, "after upload")


async def execute_fibonacci_on_device(ble_client, ble_characteristics, n):
    """Execute fibonacci(n) on the device and return the result"""
    print(f"🔢 Executing fibonacci({n}) on device...")
    
    # Get characteristics from our fixtures
    execute_char = ble_characteristics[WASM_EXECUTE_UUID]
    result_char = ble_characteristics[WASM_RESULT_UUID]
    
    # Prepare execution packet
    function_name = b"fibonacci".ljust(32, b'\x00')
    arg_count = 1
    args = [n, 0, 0, 0]
    execute_packet = function_name + struct.pack('<I', arg_count) + struct.pack('<4I', *args)
    
    # Execute function
    await ble_client.write_gatt_char(execute_char, execute_packet)
    await asyncio.sleep(0.5)  # Wait longer for execution to complete
    
    # Read the result using the result characteristic
    try:
        result_data = await ble_client.read_gatt_char(result_char)
        print(f"📊 Raw result data: {result_data.hex()}")
        
        # Parse result according to wasm_result_packet_t structure
        if len(result_data) >= 6:  # Minimum size for status + error_code + return_value
            status, error_code, return_value = struct.unpack('<BBI', result_data[:6])
            print(f"📋 Result parsed - Status: {status}, Error: {error_code}, Value: {return_value}")
            
            if status == 4:  # WASM_STATUS_COMPLETE (from the firmware)
                print(f"✅ fibonacci({n}) = {return_value}")
                return return_value
            else:
                print(f"❌ WASM execution failed - Status: {status}, Error: {error_code}")
                return None
        else:
            print(f"⚠️ Unexpected result data length: {len(result_data)}")
            return None
            
    except Exception as e:
        print(f"❌ Failed to read WASM result: {e}")
        return None


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
async def test_rust_fibonacci_e2e_single(ble_client, ble_characteristics, serial_capture):
    """Test single fibonacci calculation end-to-end"""
    # Step 1: Compile Rust to WASM
    wasm_data = compile_rust_to_wasm()
    print(f"📦 Compiled Rust Fibonacci to WASM: {len(wasm_data)} bytes")
    
    # Step 2: Upload WASM
    status, error, bytes_received = await upload_wasm_to_device(ble_client, ble_characteristics, wasm_data)
    
    if status != 3:  # Not LOADED
        pytest.fail(f"FIRMWARE BUG: WASM upload failed - Status: {status}, Error: {error}. Firmware should support {len(wasm_data)}-byte WASM binaries.")
    
    print(f"✅ WASM uploaded successfully: {bytes_received} bytes")
    
    # Step 3: Execute fibonacci(10)
    result = await execute_fibonacci_on_device(ble_client, ble_characteristics, 10)
    
    # Step 4: Verify result
    if result is not None:
        assert result == 55, f"fibonacci(10) = {result}, expected 55"
        print("🎉 Fibonacci execution successful!")
    else:
        pytest.fail("FIRMWARE BUG: Function execution failed - fibonacci(10) should return 55")
    


@pytest.mark.parametrize("n,expected", FIBONACCI_TEST_CASES)
@pytest.mark.asyncio
async def test_rust_fibonacci_e2e_comprehensive(ble_client, ble_characteristics, serial_capture, n, expected):
    """Test multiple fibonacci values end-to-end"""
    # Step 1: Compile Rust to WASM (once per test)
    wasm_data = compile_rust_to_wasm()
    
    # Step 2: Upload WASM
    status, error, bytes_received = await upload_wasm_to_device(ble_client, ble_characteristics, wasm_data)
    
    if status != 3:  # Not LOADED
        pytest.fail(f"FIRMWARE BUG: WASM upload failed for fibonacci({n}) - Status: {status}, Error: {error}. Firmware should support {len(wasm_data)}-byte WASM binaries.")
    
    # Step 3: Execute fibonacci(n)
    result = await execute_fibonacci_on_device(ble_client, ble_characteristics, n)
    
    # Step 4: Verify result
    if result is not None:
        assert result == expected, f"fibonacci({n}) = {result}, expected {expected}"
    else:
        pytest.fail(f"FIRMWARE BUG: fibonacci({n}) execution failed - should return {expected}")


if __name__ == "__main__":
    # Run via pytest when called directly
    pytest.main([__file__ + "::test_rust_fibonacci_e2e_single", "-v", "-s"])
