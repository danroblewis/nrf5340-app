#!/usr/bin/env python3
"""
Debug Packet Size Limits

Test different packet sizes to find the exact BLE limit.
"""

import asyncio
import struct
import pytest

# BLE UUIDs
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"


@pytest.mark.asyncio
async def test_debug_packet_size_limits(ble_client, ble_characteristics, serial_capture):
    """Test different packet sizes to find BLE limits"""
    
    upload_char = ble_characteristics[WASM_UPLOAD_UUID]
    status_char = ble_characteristics[WASM_STATUS_UUID]
    
    # Test sizes around the limit
    test_sizes = [240, 244, 248, 250, 251, 252, 253, 254, 255]
    
    for chunk_size in test_sizes:
        with serial_capture:
            print(f"\n🧪 Testing chunk size: {chunk_size} bytes")
            
            # Reset
            reset_packet = struct.pack('<BBHI', 0x04, 0, 0, 0)
            await ble_client.write_gatt_char(upload_char, reset_packet, response=False)
            await asyncio.sleep(0.1)
            
            # Create test WASM data
            wasm_data = b'\x00\x61\x73\x6D\x01\x00\x00\x00' + b'\x00' * (chunk_size - 8)
            total_size = len(wasm_data)
            
            # Create packet
            packet = struct.pack('<BBHI', 0x01, 0, chunk_size, total_size) + wasm_data
            packet_size = len(packet)
            
            print(f"   📦 WASM data: {len(wasm_data)} bytes")
            print(f"   📤 Packet size: {packet_size} bytes (8 header + {chunk_size} data)")
            
            try:
                await ble_client.write_gatt_char(upload_char, packet, response=False)
                print(f"   ✅ BLE write successful")
                
                await asyncio.sleep(0.2)
                
                # Check if handler was called
                serial_result = serial_capture.readouterr()
                serial_output = serial_result.out
                
                if "wasm_upload_handler called" in serial_output:
                    print(f"   ✅ Handler called successfully")
                    
                    if "Starting new upload" in serial_output:
                        print(f"   ✅ Upload started successfully")
                        
                        # Check status
                        status_data = await ble_client.read_gatt_char(status_char)
                        if len(status_data) >= 4:
                            status, error_code, bytes_received = struct.unpack('<BBH', status_data[:4])
                            if status == 1:  # RECEIVING
                                print(f"   ✅ Status: RECEIVING, Bytes: {bytes_received}")
                            elif status == 3:  # LOADED (for complete small WASM)
                                print(f"   ✅ Status: LOADED, Bytes: {bytes_received}")
                            else:
                                print(f"   ❌ Status: {status}, Error: {error_code}, Bytes: {bytes_received}")
                    else:
                        print(f"   ❌ Handler called but upload failed")
                        if "too large" in serial_output:
                            print(f"   📏 Firmware says packet too large")
                        print(f"   📋 Serial: {serial_output.strip()}")
                else:
                    print(f"   ❌ Handler NOT called")
                    if serial_output.strip():
                        print(f"   📋 Serial: {serial_output.strip()}")
                        
            except Exception as e:
                print(f"   ❌ BLE write failed: {e}")
                
        # Small delay between tests
        await asyncio.sleep(0.1)
    
    print(f"\n🎯 Test complete - check results above to find working packet sizes")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
