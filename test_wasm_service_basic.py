#!/usr/bin/env python3
"""
Basic WASM Service Test for nRF5340

This script tests basic WASM service functionality:
1. Service discovery and characteristic enumeration
2. Status reading
3. Simple WASM upload (mock data for now)
4. Result reading

Note: This is a basic connectivity and protocol test.
Real WASM binary testing will come later.

Usage:
    python3 test_wasm_service_basic.py

Requirements:
    pip install bleak
"""

import asyncio
import logging
import struct
import time
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
import sys

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(levelname)s:%(name)s:%(message)s')
logger = logging.getLogger(__name__)

# Device configuration
DEVICE_NAME = "Dan5340BLE"
DEVICE_TIMEOUT = 10.0
CONNECTION_TIMEOUT = 15.0

# WASM Service UUIDs
WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"

# WASM Commands
WASM_CMD_START_UPLOAD = 0x01
WASM_CMD_CONTINUE_UPLOAD = 0x02
WASM_CMD_END_UPLOAD = 0x03
WASM_CMD_RESET = 0x04

# WASM Status Codes
WASM_STATUS_IDLE = 0x00
WASM_STATUS_RECEIVING = 0x01
WASM_STATUS_RECEIVED = 0x02
WASM_STATUS_LOADED = 0x03
WASM_STATUS_EXECUTING = 0x04
WASM_STATUS_COMPLETE = 0x05
WASM_STATUS_ERROR = 0x06

STATUS_NAMES = {
    WASM_STATUS_IDLE: "IDLE",
    WASM_STATUS_RECEIVING: "RECEIVING",
    WASM_STATUS_RECEIVED: "RECEIVED", 
    WASM_STATUS_LOADED: "LOADED",
    WASM_STATUS_EXECUTING: "EXECUTING",
    WASM_STATUS_COMPLETE: "COMPLETE",
    WASM_STATUS_ERROR: "ERROR"
}

async def find_device():
    """Find the nRF5340 device"""
    logger.info(f"🔍 Scanning for {DEVICE_NAME}...")
    
    devices = await BleakScanner.discover(timeout=DEVICE_TIMEOUT)
    
    for device in devices:
        if device.name == DEVICE_NAME:
            logger.info(f"📱 Found device: {device.name} ({device.address})")
            return device
    
    logger.error(f"❌ Device {DEVICE_NAME} not found!")
    return None

async def test_wasm_service_discovery(client):
    """Test WASM service discovery and characteristic enumeration"""
    logger.info("🔍 Testing WASM service discovery...")
    
    # Find WASM service
    wasm_service = None
    for service in client.services:
        if service.uuid.lower() == WASM_SERVICE_UUID.lower():
            wasm_service = service
            break
    
    if not wasm_service:
        logger.error("❌ WASM service not found!")
        return False, {}
    
    logger.info(f"✅ Found WASM Service: {wasm_service.uuid}")
    
    # Enumerate characteristics
    characteristics = {}
    for char in wasm_service.characteristics:
        char_name = "Unknown"
        
        if char.uuid.lower() == WASM_UPLOAD_UUID.lower():
            char_name = "Upload"
        elif char.uuid.lower() == WASM_EXECUTE_UUID.lower():
            char_name = "Execute"
        elif char.uuid.lower() == WASM_STATUS_UUID.lower():
            char_name = "Status"
        elif char.uuid.lower() == WASM_RESULT_UUID.lower():
            char_name = "Result"
        
        characteristics[char_name.lower()] = char
        logger.info(f"  🔹 {char.uuid} ({char_name}) - Properties: {char.properties}")
    
    # Verify all expected characteristics are present
    expected_chars = ["upload", "execute", "status", "result"]
    missing_chars = [char for char in expected_chars if char not in characteristics]
    
    if missing_chars:
        logger.error(f"❌ Missing characteristics: {missing_chars}")
        return False, {}
    
    logger.info("✅ All WASM characteristics found")
    return True, characteristics

async def test_wasm_status_read(client, characteristics):
    """Test WASM status reading"""
    logger.info("📊 Testing WASM status read...")
    
    try:
        status_char = characteristics["status"]
        status_data = await client.read_gatt_char(status_char)
        
        if len(status_data) < 16:  # Expected status packet size
            logger.error(f"❌ Status response too short: {len(status_data)} bytes")
            return False
        
        # Parse status packet (simplified)
        status, error_code, bytes_received, total_size, uptime = struct.unpack('<BBHII', status_data[:12])
        
        status_name = STATUS_NAMES.get(status, f"UNKNOWN({status})")
        
        logger.info(f"  📊 Status: {status_name}")
        logger.info(f"  📊 Error Code: {error_code}")
        logger.info(f"  📊 Bytes Received: {bytes_received}")
        logger.info(f"  📊 Total Size: {total_size}")
        logger.info(f"  📊 Uptime: {uptime} seconds")
        
        logger.info("✅ Status read successful")
        return True
        
    except Exception as e:
        logger.error(f"❌ Status read failed: {e}")
        return False

async def test_wasm_reset(client, characteristics):
    """Test WASM reset command"""
    logger.info("🔄 Testing WASM reset...")
    
    try:
        upload_char = characteristics["upload"]
        
        # Create reset command packet
        # Format: cmd(1) + seq(1) + chunk_size(2) + total_size(4) + data...
        reset_packet = struct.pack('<BBHI', 
                                  WASM_CMD_RESET,  # cmd
                                  0,               # sequence
                                  0,               # chunk_size
                                  0)               # total_size
        
        await client.write_gatt_char(upload_char, reset_packet)
        logger.info("✅ Reset command sent successfully")
        
        # Give device time to process reset
        await asyncio.sleep(0.2)
        
        return True
        
    except Exception as e:
        logger.error(f"❌ Reset command failed: {e}")
        return False

async def test_wasm_mock_upload(client, characteristics):
    """Test WASM upload with valid minimal WASM binary"""
    logger.info("📤 Testing WASM upload with valid binary...")
    
    try:
        upload_char = characteristics["upload"]
        
        # Use real WASM binary compiled from WAT with add() and get_answer() functions
        mock_wasm = bytes.fromhex(
            "0061736d01000000010b0260027f7f01"
            "7f6000017f0303020001071402036164"
            "6400000a6765745f616e737765720001"
            "0a0e020700200020016a0b0400412a0b"
        )
        
        total_size = len(mock_wasm)
        logger.info(f"  📊 Mock WASM size: {total_size} bytes")
        
        # Send start upload packet
        chunk_size = min(32, total_size)  # Small chunk for test
        
        start_packet = struct.pack('<BBHI', 
                                  WASM_CMD_START_UPLOAD,  # cmd
                                  0,                      # sequence  
                                  chunk_size,             # chunk_size
                                  total_size)             # total_size
        start_packet += mock_wasm[:chunk_size]
        
        await client.write_gatt_char(upload_char, start_packet)
        logger.info(f"  ✅ Sent start upload packet ({chunk_size} bytes)")
        
        # Send remaining data if any
        bytes_sent = chunk_size
        sequence = 1
        
        while bytes_sent < total_size:
            remaining = total_size - bytes_sent
            chunk_size = min(32, remaining)
            
            continue_packet = struct.pack('<BBHI',
                                        WASM_CMD_CONTINUE_UPLOAD,  # cmd
                                        sequence,                  # sequence
                                        chunk_size,                # chunk_size
                                        total_size)                # total_size
            continue_packet += mock_wasm[bytes_sent:bytes_sent + chunk_size]
            
            await client.write_gatt_char(upload_char, continue_packet)
            logger.info(f"  ✅ Sent chunk {sequence} ({chunk_size} bytes)")
            
            bytes_sent += chunk_size
            sequence += 1
            
            # Small delay between packets
            await asyncio.sleep(0.1)
        
        logger.info(f"✅ Valid WASM upload complete ({bytes_sent}/{total_size} bytes)")
        
        # Give device time to process upload
        await asyncio.sleep(0.5)
        
        return True
        
    except Exception as e:
        logger.error(f"❌ Mock upload failed: {e}")
        return False

async def test_wasm_result_read(client, characteristics):
    """Test WASM result reading"""
    logger.info("📥 Testing WASM result read...")
    
    try:
        result_char = characteristics["result"]
        result_data = await client.read_gatt_char(result_char)
        
        if len(result_data) < 8:  # Minimum expected result size
            logger.error(f"❌ Result response too short: {len(result_data)} bytes")
            return False
        
        # Parse result packet (simplified)
        status, error_code, return_value, execution_time = struct.unpack('<BBiI', result_data[:10])
        
        status_name = STATUS_NAMES.get(status, f"UNKNOWN({status})")
        
        logger.info(f"  📊 Result Status: {status_name}")
        logger.info(f"  📊 Error Code: {error_code}")
        logger.info(f"  📊 Return Value: {return_value}")
        logger.info(f"  📊 Execution Time: {execution_time} μs")
        
        logger.info("✅ Result read successful")
        return True
        
    except Exception as e:
        logger.error(f"❌ Result read failed: {e}")
        return False

async def test_wasm_function_execution(client, characteristics):
    """Test WASM function execution"""
    logger.info("🚀 Testing WASM function execution...")
    
    try:
        execute_char = characteristics["execute"]
        
        # Create execute packet for "test" function with no arguments
        # Format: function_name[32] + arg_count[4] + args[4*4]
        function_name = b'get_answer' + b'\x00' * 22  # Pad to 32 bytes
        arg_count = struct.pack('<I', 0)       # No arguments
        args = struct.pack('<iiii', 0, 0, 0, 0)  # 4 placeholder args
        
        execute_packet = function_name + arg_count + args
        
        logger.info(f"  📤 Executing function 'get_answer' with 0 arguments")
        await client.write_gatt_char(execute_char, execute_packet)
        logger.info("  ✅ Execute command sent successfully")
        
        # Give device time to execute
        await asyncio.sleep(0.5)
        
        # Read the result and verify it's 42
        result_char = characteristics["result"]
        result_data = await client.read_gatt_char(result_char)
        
        if len(result_data) >= 10:
            status, error_code, return_value, exec_time = struct.unpack('<BBiI', result_data[:10])
            
            if error_code == 0 and return_value == 42:
                logger.info(f"  ✅ get_answer() returned {return_value} (correct!)")
                return True
            else:
                logger.error(f"  ❌ get_answer() failed: error={error_code}, value={return_value}")
                return False
        else:
            logger.error(f"  ❌ Invalid result data length: {len(result_data)}")
            return False
        
    except Exception as e:
        logger.error(f"❌ Function execution failed: {e}")
        return False

async def test_wasm_add_function(client, characteristics):
    """Test WASM add function with parameters"""
    logger.info("🧮 Testing add(5, 7) function...")
    
    try:
        execute_char = characteristics["execute"]
        
        # Create execute packet for "add" function with 2 arguments
        # Format: function_name[32] + arg_count[4] + args[4*4]
        function_name = b'add' + b'\x00' * 29  # Pad to 32 bytes
        arg_count = struct.pack('<I', 2)       # 2 arguments
        args = struct.pack('<iiii', 5, 7, 0, 0)  # add(5, 7), unused, unused
        
        execute_packet = function_name + arg_count + args
        
        logger.info(f"  📤 Executing add(5, 7)")
        await client.write_gatt_char(execute_char, execute_packet)
        logger.info("  ✅ Execute command sent successfully")
        
        # Give device time to execute
        await asyncio.sleep(0.5)
        
        # Read the result and verify it's 12
        result_char = characteristics["result"]
        result_data = await client.read_gatt_char(result_char)
        
        if len(result_data) >= 10:
            status, error_code, return_value, exec_time = struct.unpack('<BBiI', result_data[:10])
            
            if error_code == 0 and return_value == 12:
                logger.info(f"  ✅ add(5, 7) returned {return_value} (correct!)")
                return True
            else:
                logger.error(f"  ❌ add(5, 7) failed: error={error_code}, value={return_value}, expected=12")
                return False
        else:
            logger.error(f"  ❌ Invalid result data length: {len(result_data)}")
            return False
        
    except Exception as e:
        logger.error(f"❌ add() function test failed: {e}")
        return False

async def main():
    """Main test function"""
    logger.info("🚀 Starting Basic WASM Service Test")
    logger.info("=" * 60)
    
    # Find and connect to device
    device = await find_device()
    if not device:
        return False
    
    logger.info(f"🔗 Connecting to {device.address}...")
    
    try:
        async with BleakClient(device.address, timeout=CONNECTION_TIMEOUT) as client:
            logger.info(f"✅ Connected to {device.address}")
            
            # Test service discovery
            success, characteristics = await test_wasm_service_discovery(client)
            if not success:
                logger.error("❌ Service discovery failed")
                return False
            
            logger.info("")
            
            # Test status read (initial)
            success = await test_wasm_status_read(client, characteristics)
            if not success:
                logger.error("❌ Initial status read failed")
                return False
            
            logger.info("")
            
            # Test reset
            success = await test_wasm_reset(client, characteristics)
            if not success:
                logger.error("❌ Reset test failed")
                return False
            
            logger.info("")
            
            # Test status read (after reset)
            logger.info("📊 Reading status after reset...")
            success = await test_wasm_status_read(client, characteristics)
            if not success:
                logger.error("❌ Post-reset status read failed")
                return False
            
            logger.info("")
            
            # Test WASM upload
            success = await test_wasm_mock_upload(client, characteristics)
            if not success:
                logger.error("❌ WASM upload test failed")
                return False
            
            logger.info("")
            
            # Test status read (after upload)
            logger.info("📊 Reading status after upload...")
            success = await test_wasm_status_read(client, characteristics)
            if not success:
                logger.error("❌ Post-upload status read failed")
                return False
            
            logger.info("")
            
            # Test get_answer() function
            success = await test_wasm_function_execution(client, characteristics)
            if not success:
                logger.error("❌ get_answer() function test failed")
                return False
            
            logger.info("")
            
            # Test add() function with parameters
            success = await test_wasm_add_function(client, characteristics)
            if not success:
                logger.error("❌ add() function test failed")
                return False
            
            logger.info("")
            
            # Test status read (after execution)
            logger.info("📊 Reading status after execution...")
            success = await test_wasm_status_read(client, characteristics)
            if not success:
                logger.error("❌ Post-execution status read failed")
                return False
            
            logger.info("")
            
            # Test result read (should have execution result)
            logger.info("📥 Reading execution result...")
            success = await test_wasm_result_read(client, characteristics)
            if not success:
                logger.error("❌ Result read test failed")
                return False
            
            logger.info("")
            logger.info("=" * 60)
            logger.info("🎉 WASM Service Test Complete!")
            logger.info("✅ WASM upload, compilation, and execution tests passed")
            
            return True
            
    except Exception as e:
        logger.error(f"💥 Connection failed: {e}")
        return False

if __name__ == "__main__":
    try:
        success = asyncio.run(main())
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        logger.info("🛑 Test interrupted by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"💥 Unexpected error: {e}")
        sys.exit(1)
