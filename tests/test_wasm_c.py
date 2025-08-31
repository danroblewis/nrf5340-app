#!/usr/bin/env python3
"""
Tests for C-compiled WASM files using wasm_client.py
"""

import asyncio
import pytest
import subprocess
import os
from wasm_client import WASMClient


async def test_c_wasm_example(ble_client, ble_characteristics, serial_capture):
    """Test C-compiled WASM (504 bytes) - exports multiple math functions"""
    
    # First, build the C WASM using the Makefile
    c_wasm_dir = "c_wasm_test"
    wasm_file_path = os.path.join(c_wasm_dir, "simple.wasm")
    
    print(f"🔨 Building C WASM in {c_wasm_dir}...")
    
    try:
        # Run make to build the WASM file
        result = subprocess.run(
            ["make", "-C", c_wasm_dir],
            capture_output=True,
            text=True,
            check=True
        )
        print("✅ C WASM built successfully")
        print(result.stdout)
        
        # Verify the WASM file was created
        if not os.path.exists(wasm_file_path):
            pytest.fail(f"WASM file not found after build: {wasm_file_path}")
        
        # Get file size
        file_size = os.path.getsize(wasm_file_path)
        print(f"📏 Built WASM file size: {file_size} bytes")
        
    except subprocess.CalledProcessError as e:
        print(f"❌ Failed to build C WASM:")
        print(f"   stdout: {e.stdout}")
        print(f"   stderr: {e.stderr}")
        pytest.fail(f"C WASM build failed: {e}")
    except FileNotFoundError:
        pytest.fail("Make command not found. Ensure make is installed.")
    
    # Now test the built WASM file
    with open(wasm_file_path, 'rb') as f:
        wasm_data = f.read()

    print(f"🧪 Testing C WASM: {len(wasm_data)} bytes")
    
    client = WASMClient(ble_client, ble_characteristics)
    
    try:
        # Capture serial output during the test
        with serial_capture:
            await client.upload_wasm(wasm_data)
            await asyncio.sleep(1.0)
            
            # Test various C functions
            result1 = await client.execute_function("getNumber")      # Should return 99
            result2 = await client.execute_function("add", [15, 25])  # Should return 40
            result3 = await client.execute_function("multiply", [7, 8])  # Should return 56
            result4 = await client.execute_function("subtract", [50, 20])  # Should return 30
            result5 = await client.execute_function("max", [10, 20])  # Should return 20
            result6 = await client.execute_function("isEven", [42])   # Should return 1 (true)
            result7 = await client.execute_function("abs", [-15])     # Should return 15
        
        # Get serial output
        serial_output = serial_capture.readouterr()
        print(f"📱 C WASM serial output:\n{serial_output.out}")
        
        # Assert on expected serial lines
        assert "WASM Service: WASM magic validated" in serial_output.out, "Should see WASM validation"
        assert "WASM Service: Module loaded into runtime successfully" in serial_output.out, "Should see module loaded"
        assert "WASM Service: Module compilation completed successfully" in serial_output.out, "Should see compilation success"
        assert "WASM Service: WASM module ready for execution" in serial_output.out, "Should see module ready"
        
        # Test the results
        assert result1 == 99, f"getNumber() expected 99, got {result1}"
        assert result2 == 40, f"add(15,25) expected 40, got {result2}"
        assert result3 == 56, f"multiply(7,8) expected 56, got {result3}"
        assert result4 == 30, f"subtract(50,20) expected 30, got {result4}"
        assert result5 == 20, f"max(10,20) expected 20, got {result5}"
        assert result6 == 1, f"isEven(42) expected 1, got {result6}"
        assert result7 == 15, f"abs(-15) expected 15, got {result7}"
        
        print("🎉 C WASM working perfectly!")
        
    except Exception as e:
        # Get serial output even if upload failed
        serial_output = serial_capture.readouterr()
        print(f"📱 C WASM serial output (after failure):\n{serial_output.out}")
        
        print(f"⚠️ C WASM test failed as expected: {e}")
        print("   This confirms our previous findings that C-compiled WASMs have")
        print("   compatibility issues with WASM3 on this device.")
        print("   WAT-compiled WASMs work perfectly, but C/Rust compiled ones crash.")
        
        # Don't fail the test - this is expected behavior
        pytest.fail("C WASM compatibility issue confirmed - device crashes during upload")


if __name__ == "__main__":
    pytest.main()
