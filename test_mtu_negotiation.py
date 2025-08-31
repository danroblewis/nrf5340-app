#!/usr/bin/env python3
"""
MTU Negotiation and Large Packet Test Suite for nRF5340

This script tests MTU negotiation and verifies that large packets work correctly:
1. Connect to device and request maximum MTU (247 bytes)
2. Verify MTU negotiation succeeds
3. Test small packets (20 bytes) - should always work
4. Test medium packets (47 bytes) - should work with MTU 50+
5. Test large packets (244 bytes) - should work with MTU 247+
6. Verify round-trip data integrity for all packet sizes

Usage:
    python3 test_mtu_negotiation.py

Requirements:
    pip install bleak
"""

import asyncio
import logging
import struct
import time
import random
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

# Test configuration
REQUESTED_MTU = 247
MIN_MTU = 23
MEDIUM_MTU = 50
LARGE_MTU = 247

# ============================================================================
# Service and Characteristic UUIDs
# ============================================================================

# Data Service (Custom) - for MTU testing
DATA_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"
DATA_UPLOAD_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
DATA_DOWNLOAD_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"
DATA_TRANSFER_STATUS_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"

# ============================================================================
# Test Data Generation
# ============================================================================

def generate_test_data(size):
    """Generate test data of specified size with pattern for verification"""
    # Use a simple repeating pattern
    pattern = b'\xAA\xBB\xCC\xDD'
    data = (pattern * ((size // len(pattern)) + 1))[:size]
    return data

def verify_test_data(data, expected_size):
    """Verify test data integrity"""
    if len(data) != expected_size:
        return False, f"Size mismatch: got {len(data)}, expected {expected_size}"
    
    # Check pattern
    pattern = b'\xAA\xBB\xCC\xDD'
    
    for i in range(len(data)):
        expected_byte = pattern[i % len(pattern)]
        if data[i] != expected_byte:
            return False, f"Pattern mismatch at byte {i}: got 0x{data[i]:02x}, expected 0x{expected_byte:02x}"
    
    return True, "Data verified successfully"

# ============================================================================
# Device Discovery and Connection
# ============================================================================

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

# ============================================================================
# MTU Testing Functions
# ============================================================================

async def test_mtu_negotiation(client):
    """Test MTU negotiation"""
    logger.info("🔄 Testing MTU negotiation...")
    
    # Get initial MTU
    initial_mtu = client.mtu_size
    logger.info(f"📏 Initial MTU: {initial_mtu} bytes")
    
    # Try to request larger MTU (may not be supported in all Bleak versions)
    logger.info(f"📡 Requesting MTU: {REQUESTED_MTU} bytes...")
    try:
        # Some versions of Bleak don't have request_mtu, so we'll work with what we have
        if hasattr(client, 'request_mtu'):
            await client.request_mtu(REQUESTED_MTU)
        else:
            logger.info("📡 MTU request not supported by client, using negotiated MTU")
        
        negotiated_mtu = client.mtu_size
        logger.info(f"✅ MTU negotiated: {negotiated_mtu} bytes")
        
        # Calculate payload sizes
        payload_size = negotiated_mtu - 3  # ATT header is 3 bytes
        logger.info(f"📦 Max payload size: {payload_size} bytes")
        
        # Determine capabilities
        if negotiated_mtu >= LARGE_MTU:
            logger.info("🚀 Large packet support: ENABLED (244+ byte payloads)")
        elif negotiated_mtu >= MEDIUM_MTU:
            logger.info(f"📊 Medium packet support: ENABLED ({payload_size} byte payloads)")
        else:
            logger.info("⚠️  Using minimum MTU (20 byte payloads)")
        
        return negotiated_mtu, payload_size
        
    except Exception as e:
        logger.error(f"❌ MTU negotiation failed: {e}")
        return initial_mtu, initial_mtu - 3

async def test_packet_size(client, upload_char, download_char, packet_size, test_name):
    """Test a specific packet size"""
    logger.info(f"📦 Testing {test_name} ({packet_size} bytes)...")
    
    try:
        # Generate test data
        test_data = generate_test_data(packet_size)
        logger.info(f"  📤 Sending {len(test_data)} bytes...")
        
        # Write data
        await client.write_gatt_char(upload_char, test_data)
        logger.info(f"  ✅ Write successful")
        
        # Small delay to allow processing
        await asyncio.sleep(0.1)
        
        # Read data back
        logger.info(f"  📥 Reading data back...")
        received_data = await client.read_gatt_char(download_char)
        logger.info(f"  📊 Received {len(received_data)} bytes")
        
        # Verify data integrity
        is_valid, message = verify_test_data(received_data, packet_size)
        if is_valid:
            logger.info(f"  ✅ {test_name}: PASSED - {message}")
            return True
        else:
            logger.error(f"  ❌ {test_name}: FAILED - {message}")
            return False
            
    except Exception as e:
        logger.error(f"  ❌ {test_name}: FAILED - {e}")
        return False

# ============================================================================
# Main Test Function
# ============================================================================

async def test_mtu_and_packets():
    """Main test function"""
    logger.info("🚀 Starting MTU Negotiation and Large Packet Test Suite")
    logger.info("=" * 70)
    
    # Find and connect to device
    device = await find_device()
    if not device:
        return False
    
    logger.info(f"🔗 Connecting to {device.address}...")
    
    async with BleakClient(device.address, timeout=CONNECTION_TIMEOUT) as client:
        logger.info(f"✅ Connected to {device.address}")
        
        # Test MTU negotiation
        negotiated_mtu, max_payload = await test_mtu_negotiation(client)
        
        # Discover services
        logger.info("🔍 Discovering services...")
        services = client.services
        
        # Find Data Service
        data_service = None
        for service in services:
            if service.uuid.lower() == DATA_SERVICE_UUID.lower():
                data_service = service
                break
        
        if not data_service:
            logger.error(f"❌ Data Service not found!")
            return False
        
        logger.info(f"✅ Found Data Service: {data_service.uuid}")
        
        # Find characteristics
        upload_char = None
        download_char = None
        
        for char in data_service.characteristics:
            if char.uuid.lower() == DATA_UPLOAD_UUID.lower():
                upload_char = char
            elif char.uuid.lower() == DATA_DOWNLOAD_UUID.lower():
                download_char = char
        
        if not upload_char or not download_char:
            logger.error(f"❌ Required characteristics not found!")
            return False
        
        logger.info(f"✅ Found upload characteristic: {upload_char.uuid}")
        logger.info(f"✅ Found download characteristic: {download_char.uuid}")
        
        # Run packet size tests
        logger.info("")
        logger.info("📊 Running packet size tests...")
        logger.info("=" * 50)
        
        test_results = []
        
        # Test 1: Small packets (should always work)
        result = await test_packet_size(client, upload_char, download_char, 20, "Small packets")
        test_results.append(("Small packets (20 bytes)", result))
        
        # Test 2: Medium packets (should work if MTU >= 50)
        if max_payload >= 47:
            result = await test_packet_size(client, upload_char, download_char, 47, "Medium packets")
            test_results.append(("Medium packets (47 bytes)", result))
        else:
            logger.info("📦 Skipping medium packet test (MTU too small)")
            test_results.append(("Medium packets (47 bytes)", "SKIPPED"))
        
        # Test 3: Large packets (should work if MTU >= 247)
        if max_payload >= 244:
            result = await test_packet_size(client, upload_char, download_char, 244, "Large packets")
            test_results.append(("Large packets (244 bytes)", result))
        else:
            logger.info("📦 Skipping large packet test (MTU too small)")
            test_results.append(("Large packets (244 bytes)", "SKIPPED"))
        
        # Test 4: Maximum payload size test
        if max_payload > 20 and max_payload != 47 and max_payload != 244:
            result = await test_packet_size(client, upload_char, download_char, max_payload, f"Max payload")
            test_results.append((f"Max payload ({max_payload} bytes)", result))
        
        # Summary
        logger.info("")
        logger.info("=" * 70)
        logger.info("📊 TEST SUMMARY")
        logger.info("=" * 70)
        
        passed = 0
        failed = 0
        skipped = 0
        
        for test_name, result in test_results:
            if result == "SKIPPED":
                logger.info(f"⏭️  {test_name}: SKIPPED")
                skipped += 1
            elif result:
                logger.info(f"✅ {test_name}: PASSED")
                passed += 1
            else:
                logger.info(f"❌ {test_name}: FAILED")
                failed += 1
        
        logger.info("")
        logger.info(f"📊 Results: {passed} passed, {failed} failed, {skipped} skipped")
        logger.info(f"🔄 Negotiated MTU: {negotiated_mtu} bytes")
        logger.info(f"📦 Max payload: {max_payload} bytes")
        
        if failed == 0:
            logger.info("🎉 ALL TESTS PASSED!")
            return True
        else:
            logger.info(f"❌ {failed} test(s) failed")
            return False

# ============================================================================
# Main Entry Point
# ============================================================================

async def main():
    """Main entry point"""
    try:
        success = await test_mtu_and_packets()
        return 0 if success else 1
    except KeyboardInterrupt:
        logger.info("🛑 Test interrupted by user")
        return 1
    except Exception as e:
        logger.error(f"💥 Unexpected error: {e}")
        return 1

if __name__ == "__main__":
    exit_code = asyncio.run(main())
    sys.exit(exit_code)
