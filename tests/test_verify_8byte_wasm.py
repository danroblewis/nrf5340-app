#!/usr/bin/env python3
"""
Verify 8-byte WASM Actually Works

Test if the 8-byte WASM that "loads successfully" can actually execute functions.
"""

import asyncio
import struct
import pytest

# BLE UUIDs
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"


@pytest.mark.asyncio
async def test_can_8byte_wasm_execute_functions(ble_client, ble_characteristics, serial_capture):
    """Test if 8-byte WASM can actually execute functions"""
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    execute_char = ble_characteristics[WASM_EXECUTE_UUID]
    status_char = ble_characteristics[WASM_STATUS_UUID]
    result_char = ble_characteristics[WASM_RESULT_UUID]
    
    with serial_capture:
        print("\n🔍 Testing if 8-byte WASM can execute functions...")
        
        # Step 1: Reset
        print("📡 Step 1: Reset WASM service...")
        reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)
        await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
        await asyncio.sleep(0.3)
        
        # Step 2: Upload 8-byte WASM (magic + version only)
        print("📡 Step 2: Upload 8-byte WASM...")
        tiny_wasm = b'\x00\x61\x73\x6D\x01\x00\x00\x00'  # Just magic + version
        cmd = 0x01  # WASM_CMD_START_UPLOAD
        sequence = 0
        packet = struct.pack('<BBHI', cmd, sequence, len(tiny_wasm), len(tiny_wasm)) + tiny_wasm
        
        await ble_client.write_gatt_char(upload_char, packet, response=False)
        await asyncio.sleep(1.0)
        
        # Step 3: Check status - should be LOADED
        status_data = await ble_client.read_gatt_char(status_char)
        if len(status_data) >= 4:
            status, error_code, bytes_received = struct.unpack('<BBH', status_data[:4])
            print(f"📊 Status after upload: {status}, Error: {error_code}, Bytes: {bytes_received}")
            
            if status != 3:  # Not LOADED
                print(f"❌ WASM failed to load (status={status})")
                return
        
        print("✅ WASM loaded successfully, now testing function execution...")
        
        # Step 4: Try to execute a function - any function
        print("📡 Step 4: Try to execute 'test' function...")
        
        function_name = b"test".ljust(32, b'\x00')
        arg_count = 0
        args = [0, 0, 0, 0]
        execute_packet = function_name + struct.pack('<I', arg_count) + struct.pack('<4I', *args)
        
        await ble_client.write_gatt_char(execute_char, execute_packet)
        await asyncio.sleep(0.5)
        
        # Step 5: Read result
        result_data = await ble_client.read_gatt_char(result_char)
        if len(result_data) >= 6:
            status, error_code, return_value = struct.unpack('<BBI', result_data[:6])
            print(f"📋 Execution result: Status={status}, Error={error_code}, Value={return_value}")
            
            if status == 5:  # COMPLETE
                print("✅ Function executed successfully!")
            elif error_code == 5:  # FUNCTION_NOT_FOUND  
                print("⚠️ Function 'test' not found (expected - no functions in 8-byte WASM)")
            else:
                print(f"❌ Unexpected execution result: Status={status}, Error={error_code}")
        
        # Step 6: Try common function names
        for func_name in ['main', 'start', '_start', 'fibonacci', 'add']:
            print(f"📡 Step 6: Try to execute '{func_name}' function...")
            
            function_name = func_name.encode('utf-8').ljust(32, b'\x00')
            execute_packet = function_name + struct.pack('<I', 0) + struct.pack('<4I', 0, 0, 0, 0)
            
            await ble_client.write_gatt_char(execute_char, execute_packet)
            await asyncio.sleep(0.3)
            
            result_data = await ble_client.read_gatt_char(result_char)
            if len(result_data) >= 6:
                status, error_code, return_value = struct.unpack('<BBI', result_data[:6])
                
                if status == 5:  # COMPLETE
                    print(f"🎉 Function '{func_name}' executed successfully! Result: {return_value}")
                    break
                elif error_code == 5:  # FUNCTION_NOT_FOUND
                    print(f"   Function '{func_name}' not found")
                else:
                    print(f"   '{func_name}': Status={status}, Error={error_code}")
    
    # Check serial output
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    
    print(f"\n📋 Serial output captured ({len(serial_output)} chars):")
    if serial_output:
        print(serial_output[-1000:])  # Show last 1000 chars
    else:
        print("(No serial output captured)")
    
    # Analysis
    print(f"\n🎯 ANALYSIS:")
    print(f"The 8-byte WASM contains ONLY magic number + version.")
    print(f"It has NO sections, NO functions, NO code.")
    print(f"It 'loads successfully' because it's syntactically valid,")
    print(f"but it's completely useless - no executable code at all!")
    
    if "Function " in serial_output and " not found" in serial_output:
        print("✅ Confirmed: 8-byte WASM loads but has no executable functions")
    else:
        print("❓ Need to check firmware logs to understand what happened")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
