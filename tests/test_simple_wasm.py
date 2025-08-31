#!/usr/bin/env python3
"""
Simple test using wasm_client.py to upload and test the minimal WASM
"""

import asyncio
import pytest
from .wasm_client import WASMClient


async def test_minimal_wasm(ble_client, ble_characteristics, serial_capture):
    """Test the minimal WASM using the client library"""
    
    wasm_file_path = "wasm_files/compiled/minimal_test.wasm"
    with open(wasm_file_path, 'rb') as f:
        wasm_data = f.read()

    client = WASMClient(ble_client, ble_characteristics)
    
    # Capture serial output during the test
    with serial_capture:
        await client.upload_wasm(wasm_data)
        await asyncio.sleep(1.0)
        result = await client.execute_function("get_number")
    
    # Get serial output to see what happened on the device
    serial_output = serial_capture.readouterr()
    print(f"📱 Serial output:\n{serial_output}")
    
    # Assert on expected serial lines to understand the failure
    # Note: serial_output is a SerialResult object, we need to access .out
    assert "WASM Service: WASM magic validated" in serial_output.out, "Should see WASM validation"
    assert "WASM Service: Module loaded into runtime successfully" in serial_output.out, "Should see module loaded"
    assert "WASM Service: Module compilation completed successfully" in serial_output.out, "Should see compilation success"
    assert "WASM Service: WASM module ready for execution" in serial_output.out, "Should see module ready"
    
    # The result should be 42, and we're getting it successfully!
    assert result == 42, f"Expected 42, got {result}"


if __name__ == "__main__":
    pytest.main()