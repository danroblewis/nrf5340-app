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
HARDWARE_REVISION_UUID = "00002a27-0000-1000-8000-00805f9b34fb"
SOFTWARE_REVISION_UUID = "00002a28-0000-1000-8000-00805f9b34fb"


def test_device_info_service_exists(ble_services):
    """Test that Device Information Service is discovered"""
    assert DEVICE_INFO_SERVICE_UUID in ble_services


@pytest.mark.parametrize("char_uuid", [
    MANUFACTURER_NAME_UUID,
    MODEL_NUMBER_UUID,
    FIRMWARE_REVISION_UUID,
    HARDWARE_REVISION_UUID,
    SOFTWARE_REVISION_UUID,
])
async def test_device_info_characteristics_properties(ble_client, ble_characteristics, char_uuid):
    """Test that device info characteristic exists and is readable"""
    # Assert characteristic exists
    assert char_uuid in ble_characteristics, f"Characteristic {char_uuid} not found"
    
    # Assert characteristic is readable
    char = ble_characteristics[char_uuid]
    assert 'read' in char.properties, f"Characteristic {char_uuid} is not readable"

    # Assert characteristic has a value
    char = ble_characteristics[char_uuid]
    data = await ble_client.read_gatt_char(char)
    value = data.decode('utf-8').strip('\x00')
    assert len(value) > 0
