#!/usr/bin/env python3
"""
Comprehensive tests for all WAT WASM files using wasm_client.py
"""

import asyncio
import pytest
from wasm_client import WASMClient


async def test_minimal_wat_wasm(ble_client, ble_characteristics, serial_capture):
    """Test the minimal WAT WASM (43 bytes) - exports get_number() -> 42"""
    
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
    print(f"📱 Minimal WAT serial output:\n{serial_output}")
    
    # Assert on expected serial lines
    assert "WASM Service: WASM magic validated" in serial_output.out, "Should see WASM validation"
    assert "WASM Service: Module loaded into runtime successfully" in serial_output.out, "Should see module loaded"
    assert "WASM Service: Module compilation completed successfully" in serial_output.out, "Should see compilation success"
    assert "WASM Service: WASM module ready for execution" in serial_output.out, "Should see module ready"
    
    # Test the result
    assert result == 42, f"Expected 42, got {result}"


async def test_simple_no_memory_wat_wasm(ble_client, ble_characteristics, serial_capture):
    """Test simple_no_memory WAT WASM (65 bytes) - exports get_number() -> 99 and add(a,b) -> a+b"""
    
    wasm_file_path = "wasm_files/compiled/simple_no_memory.wasm"
    with open(wasm_file_path, 'rb') as f:
        wasm_data = f.read()

    client = WASMClient(ble_client, ble_characteristics)
    
    # Capture serial output during the test
    with serial_capture:
        await client.upload_wasm(wasm_data)
        await asyncio.sleep(1.0)
        
        # Test get_number function
        result1 = await client.execute_function("get_number")
        
        # Test add function with arguments
        result2 = await client.execute_function("add", [5, 7])
    
    # Get serial output
    serial_output = serial_capture.readouterr()
    print(f"📱 Simple no-memory WAT serial output:\n{serial_output}")
    
    # Assert on expected serial lines
    assert "WASM Service: WASM magic validated" in serial_output.out, "Should see WASM validation"
    assert "WASM Service: Module loaded into runtime successfully" in serial_output.out, "Should see module loaded"
    assert "WASM Service: Module compilation completed successfully" in serial_output.out, "Should see compilation success"
    assert "WASM Service: WASM module ready for execution" in serial_output.out, "Should see module ready"
    
    # Test the results
    assert result1 == 99, f"get_number() expected 99, got {result1}"
    assert result2 == 12, f"add(5,7) expected 12, got {result2}"


async def test_medium_wat_wasm(ble_client, ble_characteristics, serial_capture):
    """Test medium WAT WASM (102 bytes) - has memory import, exports get_number, add, multiply"""
    
    wasm_file_path = "wasm_files/compiled/medium_test.wasm"
    with open(wasm_file_path, 'rb') as f:
        wasm_data = f.read()

    client = WASMClient(ble_client, ble_characteristics)
    
    # Capture serial output during the test
    with serial_capture:
        await client.upload_wasm(wasm_data)
        await asyncio.sleep(1.0)
        
        # Test get_number function
        result1 = await client.execute_function("get_number")
        
        # Test add function
        result2 = await client.execute_function("add", [10, 20])
        
        # Test multiply function
        result3 = await client.execute_function("multiply", [6, 8])
    
    # Get serial output
    serial_output = serial_capture.readouterr()
    print(f"📱 Medium WAT serial output:\n{serial_output}")
    
    # Assert on expected serial lines
    assert "WASM Service: WASM magic validated" in serial_output.out, "Should see WASM validation"
    assert "WASM Service: Module loaded into runtime successfully" in serial_output.out, "Should see module loaded"
    assert "WASM Service: Module compilation completed successfully" in serial_output.out, "Should see compilation success"
    assert "WASM Service: WASM module ready for execution" in serial_output.out, "Should see module ready"
    
    # Test the results
    assert result1 == 99, f"get_number() expected 99, got {result1}"
    assert result2 == 30, f"add(10,20) expected 30, got {result2}"
    assert result3 == 48, f"multiply(6,8) expected 48, got {result3}"


async def test_large_wat_wasm(ble_client, ble_characteristics, serial_capture):
    """Test large WAT WASM (434 bytes) - has memory import, exports many get_* functions"""
    
    wasm_file_path = "wasm_files/compiled/large_test.wasm"
    with open(wasm_file_path, 'rb') as f:
        wasm_data = f.read()

    client = WASMClient(ble_client, ble_characteristics)
    
    # Capture serial output during the test
    with serial_capture:
        await client.upload_wasm(wasm_data)
        await asyncio.sleep(1.0)
        
        # Test a few representative functions
        result1 = await client.execute_function("get_number")  # Should return 99
        result2 = await client.execute_function("get_five")    # Should return 5
        result3 = await client.execute_function("get_ten")     # Should return 10
        result4 = await client.execute_function("get_twenty")  # Should return 20
    
    # Get serial output
    serial_output = serial_capture.readouterr()
    print(f"📱 Large WAT serial output:\n{serial_output}")
    
    # Assert on expected serial lines
    assert "WASM Service: WASM magic validated" in serial_output.out, "Should see WASM validation"
    assert "WASM Service: Module loaded into runtime successfully" in serial_output.out, "Should see module loaded"
    assert "WASM Service: Module compilation completed successfully" in serial_output.out, "Should see compilation success"
    assert "WASM Service: WASM module ready for execution" in serial_output.out, "Should see module ready"
    
    # Test the results
    assert result1 == 99, f"get_number() expected 99, got {result1}"
    assert result2 == 5, f"get_five() expected 5, got {result2}"
    assert result3 == 10, f"get_ten() expected 10, got {result3}"
    assert result4 == 20, f"get_twenty() expected 20, got {result4}"


if __name__ == "__main__":
    pytest.main()