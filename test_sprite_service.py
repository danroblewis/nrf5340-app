#!/usr/bin/env python3
"""
Sprite Registry Service Test Suite for nRF5340

This script tests the sprite registry service functionality:
1. Upload 16x16 monochrome bitmap sprites with CRC verification
2. Download sprites and verify data integrity
3. Test sprite verification functionality
4. Test registry status and statistics
5. Test error handling and edge cases

Usage:
    python3 test_sprite_service.py

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

# ============================================================================
# Sprite Service UUIDs
# ============================================================================

SPRITE_SERVICE_UUID = "0000fff8-0000-1000-8000-00805f9b34fb"
SPRITE_UPLOAD_UUID = "0000fff9-0000-1000-8000-00805f9b34fb"
SPRITE_DOWNLOAD_REQUEST_UUID = "0000fffa-0000-1000-8000-00805f9b34fb"
SPRITE_DOWNLOAD_RESPONSE_UUID = "0000fffb-0000-1000-8000-00805f9b34fb"
SPRITE_REGISTRY_STATUS_UUID = "0000fffc-0000-1000-8000-00805f9b34fb"
SPRITE_VERIFY_REQUEST_UUID = "0000fffd-0000-1000-8000-00805f9b34fb"
SPRITE_VERIFY_RESPONSE_UUID = "0000fffe-0000-1000-8000-00805f9b34fb"

# ============================================================================
# Sprite Constants
# ============================================================================

SPRITE_WIDTH = 16
SPRITE_HEIGHT = 16
SPRITE_DATA_SIZE = 32  # 16x16 pixels / 8 bits per byte
SPRITE_UPLOAD_PACKET_SIZE = 36  # 2 + 32 + 2 (ID + data + CRC)

# Status codes
SPRITE_STATUS_SUCCESS = 0x00
SPRITE_STATUS_NOT_FOUND = 0x01
SPRITE_STATUS_CRC_ERROR = 0x02
SPRITE_STATUS_REGISTRY_FULL = 0x03

VERIFY_STATUS_VALID = 0x00
VERIFY_STATUS_INVALID = 0x01
VERIFY_STATUS_NOT_FOUND = 0x02

# ============================================================================
# CRC16 Implementation
# ============================================================================

def calculate_crc16(data):
    """Calculate CRC16 using CCITT polynomial (0x1021)"""
    crc = 0xFFFF
    polynomial = 0x1021
    
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ polynomial
            else:
                crc <<= 1
            crc &= 0xFFFF  # Keep it 16-bit
    
    return crc

# ============================================================================
# Sprite Generation and Utilities
# ============================================================================

def create_test_sprite(sprite_id, pattern_type="checkerboard"):
    """Create a test sprite with various patterns"""
    bitmap = bytearray(SPRITE_DATA_SIZE)
    
    if pattern_type == "checkerboard":
        # Create checkerboard pattern
        for y in range(SPRITE_HEIGHT):
            for x in range(SPRITE_WIDTH):
                pixel_value = (x + y) % 2
                set_pixel(bitmap, x, y, pixel_value)
    
    elif pattern_type == "border":
        # Create border pattern
        for y in range(SPRITE_HEIGHT):
            for x in range(SPRITE_WIDTH):
                pixel_value = 1 if (x == 0 or x == 15 or y == 0 or y == 15) else 0
                set_pixel(bitmap, x, y, pixel_value)
    
    elif pattern_type == "diagonal":
        # Create diagonal lines
        for y in range(SPRITE_HEIGHT):
            for x in range(SPRITE_WIDTH):
                pixel_value = 1 if (x == y or x == (15 - y)) else 0
                set_pixel(bitmap, x, y, pixel_value)
    
    elif pattern_type == "random":
        # Create random pattern (seeded by sprite_id for reproducibility)
        random.seed(sprite_id)
        for i in range(SPRITE_DATA_SIZE):
            bitmap[i] = random.randint(0, 255)
    
    elif pattern_type == "number":
        # Create a simple number pattern based on sprite_id
        digit = sprite_id % 10
        # Simple 3x5 digit patterns (centered in 16x16)
        digit_patterns = {
            0: [0b111, 0b101, 0b101, 0b101, 0b111],
            1: [0b010, 0b110, 0b010, 0b010, 0b111],
            2: [0b111, 0b001, 0b111, 0b100, 0b111],
            3: [0b111, 0b001, 0b111, 0b001, 0b111],
            4: [0b101, 0b101, 0b111, 0b001, 0b001],
            5: [0b111, 0b100, 0b111, 0b001, 0b111],
            6: [0b111, 0b100, 0b111, 0b101, 0b111],
            7: [0b111, 0b001, 0b001, 0b001, 0b001],
            8: [0b111, 0b101, 0b111, 0b101, 0b111],
            9: [0b111, 0b101, 0b111, 0b001, 0b111],
        }
        
        pattern = digit_patterns[digit]
        start_x, start_y = 6, 5  # Center the 3x5 pattern
        
        for row, bits in enumerate(pattern):
            for col in range(3):
                if bits & (1 << (2 - col)):
                    set_pixel(bitmap, start_x + col, start_y + row, 1)
    
    return bytes(bitmap)

def set_pixel(bitmap, x, y, value):
    """Set a pixel in the bitmap"""
    if 0 <= x < SPRITE_WIDTH and 0 <= y < SPRITE_HEIGHT:
        bit_pos = y * SPRITE_WIDTH + x
        byte_idx = bit_pos // 8
        bit_offset = bit_pos % 8
        
        if value:
            bitmap[byte_idx] |= (1 << bit_offset)
        else:
            bitmap[byte_idx] &= ~(1 << bit_offset)

def get_pixel(bitmap, x, y):
    """Get a pixel from the bitmap"""
    if 0 <= x < SPRITE_WIDTH and 0 <= y < SPRITE_HEIGHT:
        bit_pos = y * SPRITE_WIDTH + x
        byte_idx = bit_pos // 8
        bit_offset = bit_pos % 8
        return (bitmap[byte_idx] >> bit_offset) & 1
    return 0

def print_sprite(bitmap, sprite_id):
    """Print sprite as ASCII art for debugging"""
    print(f"Sprite {sprite_id}:")
    for y in range(SPRITE_HEIGHT):
        line = ""
        for x in range(SPRITE_WIDTH):
            line += "██" if get_pixel(bitmap, x, y) else "  "
        print(line)
    print()

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
# Sprite Service Test Functions
# ============================================================================

async def test_sprite_upload(client, sprite_id, pattern_type="checkerboard"):
    """Test sprite upload with CRC verification"""
    logger.info(f"📤 Testing sprite upload (ID: {sprite_id}, pattern: {pattern_type})")
    
    try:
        # Find sprite service
        sprite_service = None
        for service in client.services:
            if service.uuid.lower() == SPRITE_SERVICE_UUID.lower():
                sprite_service = service
                break
        
        if not sprite_service:
            return False, "Sprite service not found"
        
        # Find upload characteristic
        upload_char = None
        for char in sprite_service.characteristics:
            if char.uuid.lower() == SPRITE_UPLOAD_UUID.lower():
                upload_char = char
                break
        
        if not upload_char:
            return False, "Upload characteristic not found"
        
        # Create test sprite
        bitmap_data = create_test_sprite(sprite_id, pattern_type)
        crc16 = calculate_crc16(bitmap_data)
        
        # Create upload packet
        upload_packet = struct.pack('<H', sprite_id) + bitmap_data + struct.pack('<H', crc16)
        
        logger.info(f"  📊 Sprite ID: {sprite_id}")
        logger.info(f"  📊 Data size: {len(bitmap_data)} bytes")
        logger.info(f"  📊 CRC16: 0x{crc16:04x}")
        logger.info(f"  📊 Packet size: {len(upload_packet)} bytes")
        
        # Upload sprite
        await client.write_gatt_char(upload_char, upload_packet)
        logger.info(f"  ✅ Upload successful")
        
        return True, bitmap_data
        
    except Exception as e:
        logger.error(f"  ❌ Upload failed: {e}")
        return False, str(e)

async def test_sprite_download(client, sprite_id):
    """Test sprite download and verification"""
    logger.info(f"📥 Testing sprite download (ID: {sprite_id})")
    
    try:
        # Find characteristics
        sprite_service = None
        for service in client.services:
            if service.uuid.lower() == SPRITE_SERVICE_UUID.lower():
                sprite_service = service
                break
        
        if not sprite_service:
            return False, None, "Sprite service not found"
        
        download_request_char = None
        download_response_char = None
        
        for char in sprite_service.characteristics:
            if char.uuid.lower() == SPRITE_DOWNLOAD_REQUEST_UUID.lower():
                download_request_char = char
            elif char.uuid.lower() == SPRITE_DOWNLOAD_RESPONSE_UUID.lower():
                download_response_char = char
        
        if not download_request_char or not download_response_char:
            return False, None, "Download characteristics not found"
        
        # Request sprite download
        request_packet = struct.pack('<H', sprite_id)
        await client.write_gatt_char(download_request_char, request_packet)
        
        # Small delay for processing
        await asyncio.sleep(0.1)
        
        # Read response
        response_data = await client.read_gatt_char(download_response_char)
        
        if len(response_data) < 37:  # 2 + 32 + 2 + 1
            return False, None, f"Response too short: {len(response_data)} bytes"
        
        # Parse response
        response_sprite_id, = struct.unpack('<H', response_data[:2])
        bitmap_data = response_data[2:34]
        response_crc16, = struct.unpack('<H', response_data[34:36])
        status = response_data[36]
        
        logger.info(f"  📊 Response sprite ID: {response_sprite_id}")
        logger.info(f"  📊 Response CRC16: 0x{response_crc16:04x}")
        logger.info(f"  📊 Status: {status}")
        
        if status != SPRITE_STATUS_SUCCESS:
            return False, None, f"Download failed with status: {status}"
        
        # Verify CRC
        calculated_crc = calculate_crc16(bitmap_data)
        if calculated_crc != response_crc16:
            return False, None, f"CRC mismatch: got 0x{calculated_crc:04x}, expected 0x{response_crc16:04x}"
        
        logger.info(f"  ✅ Download successful, CRC verified")
        return True, bitmap_data, "Success"
        
    except Exception as e:
        logger.error(f"  ❌ Download failed: {e}")
        return False, None, str(e)

async def test_sprite_verification(client, sprite_id):
    """Test sprite CRC verification"""
    logger.info(f"🔍 Testing sprite verification (ID: {sprite_id})")
    
    try:
        # Find characteristics
        sprite_service = None
        for service in client.services:
            if service.uuid.lower() == SPRITE_SERVICE_UUID.lower():
                sprite_service = service
                break
        
        verify_request_char = None
        verify_response_char = None
        
        for char in sprite_service.characteristics:
            if char.uuid.lower() == SPRITE_VERIFY_REQUEST_UUID.lower():
                verify_request_char = char
            elif char.uuid.lower() == SPRITE_VERIFY_RESPONSE_UUID.lower():
                verify_response_char = char
        
        if not verify_request_char or not verify_response_char:
            return False, "Verification characteristics not found"
        
        # Request verification
        request_packet = struct.pack('<H', sprite_id)
        await client.write_gatt_char(verify_request_char, request_packet)
        
        # Small delay for processing
        await asyncio.sleep(0.1)
        
        # Read response
        response_data = await client.read_gatt_char(verify_response_char)
        
        if len(response_data) < 8:
            return False, f"Response too short: {len(response_data)} bytes"
        
        # Parse response
        response_sprite_id, stored_crc16, calculated_crc16, verify_status = struct.unpack('<HHHB', response_data[:7])
        
        logger.info(f"  📊 Response sprite ID: {response_sprite_id}")
        logger.info(f"  📊 Stored CRC16: 0x{stored_crc16:04x}")
        logger.info(f"  📊 Calculated CRC16: 0x{calculated_crc16:04x}")
        logger.info(f"  📊 Verification status: {verify_status}")
        
        if verify_status == VERIFY_STATUS_VALID:
            logger.info(f"  ✅ Verification successful")
            return True, "Verification passed"
        elif verify_status == VERIFY_STATUS_NOT_FOUND:
            logger.info(f"  ⚠️  Sprite not found")
            return True, "Sprite not found"
        else:
            logger.error(f"  ❌ Verification failed")
            return False, f"Verification failed with status: {verify_status}"
        
    except Exception as e:
        logger.error(f"  ❌ Verification failed: {e}")
        return False, str(e)

async def test_registry_status(client):
    """Test registry status reading"""
    logger.info(f"📊 Testing registry status")
    
    try:
        # Find status characteristic
        sprite_service = None
        for service in client.services:
            if service.uuid.lower() == SPRITE_SERVICE_UUID.lower():
                sprite_service = service
                break
        
        status_char = None
        for char in sprite_service.characteristics:
            if char.uuid.lower() == SPRITE_REGISTRY_STATUS_UUID.lower():
                status_char = char
                break
        
        if not status_char:
            return False, "Status characteristic not found"
        
        # Read status
        status_data = await client.read_gatt_char(status_char)
        
        if len(status_data) < 12:
            return False, f"Status response too short: {len(status_data)} bytes"
        
        # Parse status
        total_sprites, free_slots, last_sprite_id, registry_status, last_operation, crc_errors = struct.unpack('<HHHBBH', status_data[:10])
        
        logger.info(f"  📊 Total sprites: {total_sprites}")
        logger.info(f"  📊 Free slots: {free_slots}")
        logger.info(f"  📊 Last sprite ID: {last_sprite_id}")
        logger.info(f"  📊 Registry status: {registry_status}")
        logger.info(f"  📊 Last operation: {last_operation}")
        logger.info(f"  📊 CRC errors: {crc_errors}")
        
        logger.info(f"  ✅ Status read successful")
        return True, {
            'total_sprites': total_sprites,
            'free_slots': free_slots,
            'last_sprite_id': last_sprite_id,
            'registry_status': registry_status,
            'crc_errors': crc_errors
        }
        
    except Exception as e:
        logger.error(f"  ❌ Status read failed: {e}")
        return False, str(e)

# ============================================================================
# Main Test Function
# ============================================================================

async def test_sprite_service():
    """Main test function"""
    logger.info("🚀 Starting Sprite Registry Service Test Suite")
    logger.info("=" * 70)
    
    # Find and connect to device
    device = await find_device()
    if not device:
        return False
    
    logger.info(f"🔗 Connecting to {device.address}...")
    
    async with BleakClient(device.address, timeout=CONNECTION_TIMEOUT) as client:
        logger.info(f"✅ Connected to {device.address}")
        
        # Check if sprite service exists
        sprite_service = None
        for service in client.services:
            if service.uuid.lower() == SPRITE_SERVICE_UUID.lower():
                sprite_service = service
                break
        
        if not sprite_service:
            logger.error("❌ Sprite service not found!")
            return False
        
        logger.info(f"✅ Found Sprite Service: {sprite_service.uuid}")
        
        # List characteristics
        logger.info("📋 Available characteristics:")
        for char in sprite_service.characteristics:
            char_name = "Unknown"
            if char.uuid.lower() == SPRITE_UPLOAD_UUID.lower():
                char_name = "Upload"
            elif char.uuid.lower() == SPRITE_DOWNLOAD_REQUEST_UUID.lower():
                char_name = "Download Request"
            elif char.uuid.lower() == SPRITE_DOWNLOAD_RESPONSE_UUID.lower():
                char_name = "Download Response"
            elif char.uuid.lower() == SPRITE_REGISTRY_STATUS_UUID.lower():
                char_name = "Registry Status"
            elif char.uuid.lower() == SPRITE_VERIFY_REQUEST_UUID.lower():
                char_name = "Verify Request"
            elif char.uuid.lower() == SPRITE_VERIFY_RESPONSE_UUID.lower():
                char_name = "Verify Response"
            
            logger.info(f"  🔹 {char.uuid} ({char_name}) - Properties: {char.properties}")
        
        logger.info("")
        
        # Test registry status (initial)
        logger.info("📊 Initial registry status:")
        success, status_data = await test_registry_status(client)
        if not success:
            logger.error(f"❌ Failed to read initial status: {status_data}")
            return False
        
        logger.info("")
        
        # Test sprite uploads
        test_sprites = [
            (1, "checkerboard"),
            (2, "border"),
            (3, "diagonal"),
            (42, "number"),
            (100, "random"),
        ]
        
        uploaded_sprites = {}
        
        for sprite_id, pattern in test_sprites:
            success, bitmap_data = await test_sprite_upload(client, sprite_id, pattern)
            if success:
                uploaded_sprites[sprite_id] = bitmap_data
                logger.info(f"  ✅ Sprite {sprite_id} uploaded successfully")
            else:
                logger.error(f"  ❌ Sprite {sprite_id} upload failed: {bitmap_data}")
            
            logger.info("")
        
        # Test sprite downloads and verification
        for sprite_id in uploaded_sprites:
            # Test download
            success, downloaded_data, message = await test_sprite_download(client, sprite_id)
            if success:
                # Verify data matches
                if downloaded_data == uploaded_sprites[sprite_id]:
                    logger.info(f"  ✅ Sprite {sprite_id} download and data verification successful")
                else:
                    logger.error(f"  ❌ Sprite {sprite_id} data mismatch!")
            else:
                logger.error(f"  ❌ Sprite {sprite_id} download failed: {message}")
            
            # Test verification
            success, message = await test_sprite_verification(client, sprite_id)
            if success:
                logger.info(f"  ✅ Sprite {sprite_id} verification: {message}")
            else:
                logger.error(f"  ❌ Sprite {sprite_id} verification failed: {message}")
            
            logger.info("")
        
        # Test non-existent sprite
        logger.info("🔍 Testing non-existent sprite (ID: 9999):")
        success, downloaded_data, message = await test_sprite_download(client, 9999)
        if not success and "status: 1" in message:  # SPRITE_STATUS_NOT_FOUND
            logger.info(f"  ✅ Correctly handled non-existent sprite")
        else:
            logger.error(f"  ❌ Unexpected result for non-existent sprite: {message}")
        
        success, message = await test_sprite_verification(client, 9999)
        if success and "not found" in message.lower():
            logger.info(f"  ✅ Correctly handled non-existent sprite verification")
        else:
            logger.error(f"  ❌ Unexpected verification result: {message}")
        
        logger.info("")
        
        # Final registry status
        logger.info("📊 Final registry status:")
        success, final_status = await test_registry_status(client)
        if success:
            logger.info(f"  ✅ Final status read successful")
        else:
            logger.error(f"  ❌ Final status read failed: {final_status}")
        
        logger.info("")
        logger.info("=" * 70)
        logger.info("🎉 Sprite Registry Service Test Complete!")
        logger.info(f"📊 Uploaded {len(uploaded_sprites)} sprites successfully")
        
        return True

# ============================================================================
# Main Entry Point
# ============================================================================

async def main():
    """Main entry point"""
    try:
        success = await test_sprite_service()
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
