#!/usr/bin/env python3
"""
Comprehensive BLE Test Suite for nRF5340

This script combines all BLE tests into a single comprehensive test suite:
- Device discovery and connection
- Device Information Service tests  
- Custom services functional tests (Control, Data, DFU)
- Data Service round-trip verification

Usage:
    python3 test_ble_comprehensive.py

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

# ============================================================================
# Service and Characteristic UUIDs
# ============================================================================

# Device Information Service (Standard)
DEVICE_INFO_SERVICE_UUID = "0000180a-0000-1000-8000-00805f9b34fb"
MANUFACTURER_NAME_UUID = "00002a29-0000-1000-8000-00805f9b34fb"
MODEL_NUMBER_UUID = "00002a24-0000-1000-8000-00805f9b34fb"
FIRMWARE_REVISION_UUID = "00002a26-0000-1000-8000-00805f9b34fb"
HARDWARE_REVISION_UUID = "00002a27-0000-1000-8000-00805f9b34fb"
SOFTWARE_REVISION_UUID = "00002a28-0000-1000-8000-00805f9b34fb"

# Control Service (Custom)
CONTROL_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"
CONTROL_COMMAND_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
CONTROL_RESPONSE_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
CONTROL_STATUS_UUID = "0000ffe3-0000-1000-8000-00805f9b34fb"

# Data Service (Custom)
DATA_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"
DATA_UPLOAD_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
DATA_DOWNLOAD_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"
DATA_TRANSFER_STATUS_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"

# DFU Service (Custom)
DFU_SERVICE_UUID = "0000fe59-0000-1000-8000-00805f9b34fb"
DFU_CONTROL_POINT_UUID = "0000ffd0-0000-1000-8000-00805f9b34fb"
DFU_PACKET_UUID = "0000ffd1-0000-1000-8000-00805f9b34fb"

# ============================================================================
# Test Result Tracking
# ============================================================================

class TestResults:
    def __init__(self):
        self.tests_run = 0
        self.tests_passed = 0
        self.tests_failed = 0
        self.failures = []
    
    def add_result(self, test_name, passed, error_msg=None):
        self.tests_run += 1
        if passed:
            self.tests_passed += 1
            logger.info(f"✅ {test_name}: PASSED")
        else:
            self.tests_failed += 1
            self.failures.append(f"{test_name}: {error_msg}")
            logger.error(f"❌ {test_name}: FAILED - {error_msg}")
    
    def summary(self):
        logger.info("="*60)
        logger.info(f"📊 TEST SUMMARY")
        logger.info("="*60)
        logger.info(f"Total tests: {self.tests_run}")
        logger.info(f"✅ Passed: {self.tests_passed}")
        logger.info(f"❌ Failed: {self.tests_failed}")
        
        if self.failures:
            logger.info("\nFailure details:")
            for failure in self.failures:
                logger.info(f"  - {failure}")
        
        success_rate = (self.tests_passed / self.tests_run * 100) if self.tests_run > 0 else 0
        logger.info(f"\n🎯 Success rate: {success_rate:.1f}%")
        
        if self.tests_failed == 0:
            logger.info("🎉 ALL TESTS PASSED!")
            return True
        else:
            logger.info(f"⚠️  {self.tests_failed} test(s) failed")
            return False

# ============================================================================
# Device Discovery and Connection
# ============================================================================

async def find_device():
    """Find and return the nRF5340 device"""
    logger.info("🔍 Scanning for nRF5340 device...")
    
    try:
        devices = await BleakScanner.discover(timeout=DEVICE_TIMEOUT)
        
        for device in devices:
            if device.name == DEVICE_NAME:
                logger.info(f"📱 Found device: {device.name} ({device.address})")
                return device
        
        logger.error(f"❌ Device '{DEVICE_NAME}' not found")
        logger.info("Available devices:")
        for device in devices:
            logger.info(f"  - {device.name or 'Unknown'} ({device.address})")
        
        return None
        
    except Exception as e:
        logger.error(f"❌ Error during device scan: {e}")
        return None

# ============================================================================
# Device Information Service Tests
# ============================================================================

async def test_device_info_service(client, results):
    """Test all Device Information Service characteristics"""
    logger.info("🔧 Testing Device Information Service...")
    
    try:
        # Find Device Information Service
        device_info_service = None
        for service in client.services:
            if service.uuid.lower() == DEVICE_INFO_SERVICE_UUID.lower():
                device_info_service = service
                break
        
        if not device_info_service:
            results.add_result("Device Info Service Discovery", False, "Service not found")
            return
        
        results.add_result("Device Info Service Discovery", True)
        
        # Test characteristics
        char_tests = [
            ("Manufacturer Name", MANUFACTURER_NAME_UUID),
            ("Model Number", MODEL_NUMBER_UUID),
            ("Firmware Revision", FIRMWARE_REVISION_UUID),
            ("Hardware Revision", HARDWARE_REVISION_UUID),
            ("Software Revision", SOFTWARE_REVISION_UUID),
        ]
        
        for char_name, char_uuid in char_tests:
            try:
                characteristic = None
                for char in device_info_service.characteristics:
                    if char.uuid.lower() == char_uuid.lower():
                        characteristic = char
                        break
                
                if characteristic:
                    value = await client.read_gatt_char(characteristic)
                    text_value = value.decode('utf-8').rstrip('\x00')
                    logger.info(f"  📝 {char_name}: {text_value}")
                    results.add_result(f"Device Info - {char_name}", True)
                else:
                    results.add_result(f"Device Info - {char_name}", False, "Characteristic not found")
                    
            except Exception as e:
                results.add_result(f"Device Info - {char_name}", False, str(e))
                
    except Exception as e:
        results.add_result("Device Info Service Test", False, str(e))

# ============================================================================
# Control Service Tests
# ============================================================================

async def test_control_service(client, results):
    """Test Control Service functionality"""
    logger.info("🎛️  Testing Control Service...")
    
    try:
        # Find Control Service
        control_service = None
        for service in client.services:
            if service.uuid.lower() == CONTROL_SERVICE_UUID.lower():
                control_service = service
                break
        
        if not control_service:
            results.add_result("Control Service Discovery", False, "Service not found")
            return
        
        results.add_result("Control Service Discovery", True)
        
        # Find characteristics
        command_char = None
        response_char = None
        status_char = None
        
        for char in control_service.characteristics:
            if char.uuid.lower() == CONTROL_COMMAND_UUID.lower():
                command_char = char
            elif char.uuid.lower() == CONTROL_RESPONSE_UUID.lower():
                response_char = char
            elif char.uuid.lower() == CONTROL_STATUS_UUID.lower():
                status_char = char
        
        # Test command characteristic
        if command_char:
            try:
                # Send GET_STATUS command (0x01) - 20 bytes total to match control_command_packet_t
                command_packet = struct.pack('<BBB', 0x01, 0x00, 0x00) + b'\x00' * 17  # cmd=0x01, param1=0x00, param2=0x00 + 17 reserved bytes
                await client.write_gatt_char(command_char, command_packet)
                logger.info("  📤 Sent GET_STATUS command")
                results.add_result("Control - Command Write", True)
                
                # Small delay for processing
                await asyncio.sleep(0.1)
                
            except Exception as e:
                results.add_result("Control - Command Write", False, str(e))
        else:
            results.add_result("Control - Command Characteristic", False, "Characteristic not found")
        
        # Test response characteristic
        if response_char:
            try:
                response = await client.read_gatt_char(response_char)
                logger.info(f"  📥 Response received: {len(response)} bytes")
                results.add_result("Control - Response Read", True)
            except Exception as e:
                results.add_result("Control - Response Read", False, str(e))
        else:
            results.add_result("Control - Response Characteristic", False, "Characteristic not found")
        
        # Test status characteristic
        if status_char:
            try:
                status = await client.read_gatt_char(status_char)
                logger.info(f"  📊 Status received: {len(status)} bytes")
                results.add_result("Control - Status Read", True)
            except Exception as e:
                results.add_result("Control - Status Read", False, str(e))
        else:
            results.add_result("Control - Status Characteristic", False, "Characteristic not found")
                
    except Exception as e:
        results.add_result("Control Service Test", False, str(e))

# ============================================================================
# Data Service Round-Trip Test
# ============================================================================

async def test_data_service_roundtrip(client, results):
    """Test Data Service with round-trip verification"""
    logger.info("🔄 Testing Data Service Round-Trip...")
    
    try:
        # Find Data Service
        data_service = None
        for service in client.services:
            if service.uuid.lower() == DATA_SERVICE_UUID.lower():
                data_service = service
                break
        
        if not data_service:
            results.add_result("Data Service Discovery", False, "Service not found")
            return
        
        results.add_result("Data Service Discovery", True)
        
        # Find characteristics
        upload_char = None
        download_char = None
        
        for char in data_service.characteristics:
            if char.uuid.lower() == DATA_UPLOAD_UUID.lower():
                upload_char = char
            elif char.uuid.lower() == DATA_DOWNLOAD_UUID.lower():
                download_char = char
        
        if not upload_char:
            results.add_result("Data - Upload Characteristic", False, "Characteristic not found")
            return
        
        if not download_char:
            results.add_result("Data - Download Characteristic", False, "Characteristic not found")
            return
        
        # Generate test timestamp
        test_timestamp = int(time.time())
        logger.info(f"  📊 Test timestamp: {test_timestamp}")
        
        # Create test data packet (20 bytes to match data_upload_packet_t)
        test_data = struct.pack('<I', test_timestamp) + b'\x00' * 16  # 4 bytes timestamp + 16 bytes padding
        
        # Upload data
        try:
            logger.info(f"  💾 Writing {len(test_data)} bytes to Data Upload...")
            await client.write_gatt_char(upload_char, test_data)
            logger.info("  ✅ Write successful!")
            results.add_result("Data - Upload Write", True)
        except Exception as e:
            results.add_result("Data - Upload Write", False, str(e))
            return
        
        # Small delay to let the device process
        await asyncio.sleep(0.1)
        
        # Download data
        try:
            logger.info("  📖 Reading from Data Download...")
            response_data = await client.read_gatt_char(download_char)
            logger.info(f"  ✅ Read {len(response_data)} bytes")
            results.add_result("Data - Download Read", True)
            
            # Parse response (first 4 bytes should be our timestamp)
            if len(response_data) >= 4:
                received_timestamp = struct.unpack('<I', response_data[:4])[0]
                logger.info(f"  📊 Received timestamp: {received_timestamp}")
                
                # Verify round-trip
                if received_timestamp == test_timestamp:
                    logger.info("  🎉 Round-trip verification: SUCCESS!")
                    results.add_result("Data - Round-trip Verification", True)
                else:
                    results.add_result("Data - Round-trip Verification", False, 
                                     f"Expected {test_timestamp}, got {received_timestamp}")
            else:
                results.add_result("Data - Round-trip Verification", False, 
                                 f"Response too short: {len(response_data)} bytes")
                
        except Exception as e:
            results.add_result("Data - Download Read", False, str(e))
            
    except Exception as e:
        results.add_result("Data Service Test", False, str(e))

# ============================================================================
# DFU Service Tests
# ============================================================================

async def test_dfu_service(client, results):
    """Test DFU Service functionality"""
    logger.info("🔄 Testing DFU Service...")
    
    try:
        # Find DFU Service
        dfu_service = None
        for service in client.services:
            if service.uuid.lower() == DFU_SERVICE_UUID.lower():
                dfu_service = service
                break
        
        if not dfu_service:
            results.add_result("DFU Service Discovery", False, "Service not found")
            return
        
        results.add_result("DFU Service Discovery", True)
        
        # Find characteristics
        control_char = None
        packet_char = None
        
        for char in dfu_service.characteristics:
            if char.uuid.lower() == DFU_CONTROL_POINT_UUID.lower():
                control_char = char
            elif char.uuid.lower() == DFU_PACKET_UUID.lower():
                packet_char = char
        
        # Test control point characteristic
        if control_char:
            try:
                # Send a simple command (e.g., command 0x01 for testing) - 20 bytes total to match dfu_control_packet_t
                command_packet = struct.pack('<B', 0x01) + b'\x00' * 19  # 1 byte command + 19 bytes parameters
                await client.write_gatt_char(control_char, command_packet)
                logger.info("  📤 Sent DFU control command")
                results.add_result("DFU - Control Point Write", True)
            except Exception as e:
                results.add_result("DFU - Control Point Write", False, str(e))
        else:
            results.add_result("DFU - Control Point Characteristic", False, "Characteristic not found")
        
        # Test packet characteristic (just verify it exists and is writable)
        if packet_char:
            logger.info("  📦 DFU Packet characteristic found")
            results.add_result("DFU - Packet Characteristic", True)
        else:
            results.add_result("DFU - Packet Characteristic", False, "Characteristic not found")
                
    except Exception as e:
        results.add_result("DFU Service Test", False, str(e))

# ============================================================================
# Main Test Runner
# ============================================================================

async def run_comprehensive_tests():
    """Run all BLE tests in sequence"""
    results = TestResults()
    
    logger.info("🚀 Starting Comprehensive BLE Test Suite")
    logger.info("="*60)
    
    # Find device
    device = await find_device()
    if not device:
        results.add_result("Device Discovery", False, "Device not found")
        results.summary()
        return False
    
    results.add_result("Device Discovery", True)
    
    # Connect to device
    try:
        logger.info(f"🔗 Connecting to {device.address}...")
        async with BleakClient(device.address, timeout=CONNECTION_TIMEOUT) as client:
            logger.info(f"✅ Connected to {device.address}")
            results.add_result("Device Connection", True)
            
            # List all available services
            logger.info("📋 Discovered services:")
            for service in client.services:
                service_name = "Unknown"
                if service.uuid.lower() == DEVICE_INFO_SERVICE_UUID.lower():
                    service_name = "Device Information"
                elif service.uuid.lower() == CONTROL_SERVICE_UUID.lower():
                    service_name = "Control Service"
                elif service.uuid.lower() == DATA_SERVICE_UUID.lower():
                    service_name = "Data Service"
                elif service.uuid.lower() == DFU_SERVICE_UUID.lower():
                    service_name = "DFU Service"
                
                logger.info(f"  🔹 {service.uuid} ({service_name})")
            
            logger.info("")
            
            # Run all service tests
            await test_device_info_service(client, results)
            logger.info("")
            
            await test_control_service(client, results)
            logger.info("")
            
            await test_data_service_roundtrip(client, results)
            logger.info("")
            
            await test_dfu_service(client, results)
            logger.info("")
            
    except Exception as e:
        logger.error(f"❌ Connection failed: {e}")
        results.add_result("Device Connection", False, str(e))
    
    # Show final results
    return results.summary()

async def main():
    """Main entry point"""
    try:
        success = await run_comprehensive_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        logger.info("\n⏹️  Test interrupted by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"❌ Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    asyncio.run(main())
