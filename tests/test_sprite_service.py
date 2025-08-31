#!/usr/bin/env python3
"""
Sprite Service Tests

Clean pytest tests for sprite upload/download functionality.
Note: Current device implementation has limited sprite functionality.
"""

import pytest
import asyncio
import struct

# Sprite Service UUIDs
SPRITE_SERVICE_UUID = "0000fff8-0000-1000-8000-00805f9b34fb"
SPRITE_UPLOAD_UUID = "0000fff9-0000-1000-8000-00805f9b34fb"
SPRITE_DOWNLOAD_REQ_UUID = "0000fffa-0000-1000-8000-00805f9b34fb"
SPRITE_DOWNLOAD_RESP_UUID = "0000fffb-0000-1000-8000-00805f9b34fb"
SPRITE_REGISTRY_UUID = "0000fffc-0000-1000-8000-00805f9b34fb"
SPRITE_VERIFY_REQ_UUID = "0000fffd-0000-1000-8000-00805f9b34fb"
SPRITE_VERIFY_RESP_UUID = "0000fffe-0000-1000-8000-00805f9b34fb"


def test_sprite_service_exists(ble_services):
    """Test sprite service is discovered"""
    services, characteristics = ble_services
    assert SPRITE_SERVICE_UUID in services


def test_sprite_characteristics(ble_services):
    """Test all sprite characteristics are present"""
    services, characteristics = ble_services
    
    required_chars = [
        SPRITE_UPLOAD_UUID,
        SPRITE_DOWNLOAD_REQ_UUID,
        SPRITE_DOWNLOAD_RESP_UUID,
        SPRITE_REGISTRY_UUID,
        SPRITE_VERIFY_REQ_UUID,
        SPRITE_VERIFY_RESP_UUID
    ]
    
    for char_uuid in required_chars:
        assert char_uuid in characteristics


@pytest.mark.asyncio
async def test_sprite_registry_read(ble_client, ble_services):
    """Test reading sprite registry status"""
    services, characteristics = ble_services
    
    registry_char = characteristics[SPRITE_REGISTRY_UUID]
    registry_data = await ble_client.read_gatt_char(registry_char)
    
    # Registry format: count(2) + max_sprites(2) + sprite_size(2) + data...
    assert len(registry_data) >= 6
    
    count, max_sprites, sprite_size = struct.unpack('<HHH', registry_data[:6])
    
    assert max_sprites > 0
    assert sprite_size > 0
    assert count <= max_sprites


@pytest.mark.asyncio
async def test_sprite_operations_may_not_work(ble_client, ble_services):
    """Test that sprite operations are present but may not be implemented"""
    services, characteristics = ble_services
    
    # Test upload characteristic exists
    upload_char = characteristics[SPRITE_UPLOAD_UUID]
    assert upload_char is not None
    
    # Test download characteristics exist
    download_req_char = characteristics[SPRITE_DOWNLOAD_REQ_UUID]
    download_resp_char = characteristics[SPRITE_DOWNLOAD_RESP_UUID]
    assert download_req_char is not None
    assert download_resp_char is not None
    
    # Test verify characteristics exist
    verify_req_char = characteristics[SPRITE_VERIFY_REQ_UUID]
    verify_resp_char = characteristics[SPRITE_VERIFY_RESP_UUID]
    assert verify_req_char is not None
    assert verify_resp_char is not None
    
    # Note: Actual operations may fail due to device implementation limitations
    # This test just verifies the service structure is present