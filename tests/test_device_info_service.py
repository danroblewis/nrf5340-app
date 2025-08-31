#!/usr/bin/env python3
"""
Device Information Service Tests

Tests for BLE Device Information Service (0x180A)
"""

import pytest

# Service UUIDs
DEVICE_INFO_SERVICE_UUID = "0000180a-0000-1000-8000-00805f9b34fb"

# Characteristic UUIDs
MANUFACTURER_NAME_UUID = "00002a29-0000-1000-8000-00805f9b34fb"
MODEL_NUMBER_UUID = "00002a24-0000-1000-8000-00805f9b34fb"
FIRMWARE_REVISION_UUID = "00002a26-0000-1000-8000-00805f9b34fb"


def test_device_info_service_exists(ble_services):
    """Test that Device Information Service is discovered"""
    services, characteristics = ble_services
    assert DEVICE_INFO_SERVICE_UUID in services


@pytest.mark.asyncio
async def test_device_info_characteristics(ble_client, ble_services):
    """Test reading device information characteristics"""
    services, characteristics = ble_services
    
    assert DEVICE_INFO_SERVICE_UUID in services
    
    # Test manufacturer name
    if MANUFACTURER_NAME_UUID in characteristics:
        char = characteristics[MANUFACTURER_NAME_UUID]
        data = await ble_client.read_gatt_char(char)
        manufacturer = data.decode('utf-8').strip('\x00')
        assert len(manufacturer) > 0
    
    # Test model number
    if MODEL_NUMBER_UUID in characteristics:
        char = characteristics[MODEL_NUMBER_UUID]
        data = await ble_client.read_gatt_char(char)
        model = data.decode('utf-8').strip('\x00')
        assert len(model) > 0
    
    # Test firmware revision
    if FIRMWARE_REVISION_UUID in characteristics:
        char = characteristics[FIRMWARE_REVISION_UUID]
        data = await ble_client.read_gatt_char(char)
        firmware = data.decode('utf-8').strip('\x00')
        assert len(firmware) > 0


def test_device_info_characteristics_properties(ble_services):
    """Test that device info characteristics have expected properties"""
    services, characteristics = ble_services
    
    if MANUFACTURER_NAME_UUID in characteristics:
        char = characteristics[MANUFACTURER_NAME_UUID]
        assert 'read' in char.properties
