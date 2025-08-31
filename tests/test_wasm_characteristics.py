#!/usr/bin/env python3
"""
WASM Service Characteristic Properties Tests

Tests to verify the actual BLE characteristics properties for WASM service
and identify any mismatches between firmware code and actual implementation.
"""

import pytest

# WASM Service UUIDs (matching firmware wasm_service.h)
WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"  # Fixed: was fff3, now fff6
WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"  # Fixed: was fff6, now fff3


def test_wasm_service_exists(ble_services):
    """Test that WASM Service is discovered"""
    assert WASM_SERVICE_UUID in ble_services, "WASM Service not found in discovered services"


@pytest.mark.parametrize("char_uuid,expected_properties", [
    (WASM_UPLOAD_UUID, ['write', 'write-without-response']),
    (WASM_EXECUTE_UUID, ['write']),
    (WASM_STATUS_UUID, ['read', 'notify']),
    (WASM_RESULT_UUID, ['read', 'notify']),
])
def test_wasm_characteristic_properties(ble_characteristics, char_uuid, expected_properties):
    """Test WASM characteristic properties match firmware expectations"""
    # Assert characteristic exists
    assert char_uuid in ble_characteristics, f"WASM characteristic {char_uuid} not found"
    
    # Get characteristic and its properties
    char = ble_characteristics[char_uuid]
    actual_properties = char.properties
    
    print(f"\n📋 Characteristic {char_uuid}:")
    print(f"   Expected: {expected_properties}")
    print(f"   Actual:   {list(actual_properties)}")
    
    # Check each expected property
    for prop in expected_properties:
        assert prop in actual_properties, f"Characteristic {char_uuid} missing expected property '{prop}'. Actual properties: {list(actual_properties)}"


@pytest.mark.asyncio
async def test_wasm_status_readable(ble_client, ble_characteristics):
    """Test that WASM status characteristic is readable"""
    char = ble_characteristics[WASM_STATUS_UUID]
    
    # Should be able to read status
    try:
        status_data = await ble_client.read_gatt_char(char)
        print(f"📊 WASM Status data: {status_data.hex()}")
        assert len(status_data) > 0, "WASM status returned empty data"
        print("✅ WASM status characteristic read successfully")
    except Exception as e:
        pytest.fail(f"Failed to read WASM status characteristic: {e}")


@pytest.mark.asyncio
async def test_wasm_result_readable(ble_client, ble_characteristics):
    """Test that WASM result characteristic is readable"""
    char = ble_characteristics[WASM_RESULT_UUID]
    
    # Should be able to read result (even if no execution has occurred)
    try:
        result_data = await ble_client.read_gatt_char(char)
        print(f"📊 WASM Result data: {result_data.hex()}")
        assert len(result_data) > 0, "WASM result returned empty data"
        print("✅ WASM result characteristic read successfully")
    except Exception as e:
        pytest.fail(f"Failed to read WASM result characteristic: {e}")


def test_wasm_all_characteristics_found(ble_characteristics):
    """Test that all expected WASM characteristics are discovered"""
    expected_chars = [
        WASM_UPLOAD_UUID,
        WASM_EXECUTE_UUID, 
        WASM_STATUS_UUID,
        WASM_RESULT_UUID
    ]
    
    for char_uuid in expected_chars:
        assert char_uuid in ble_characteristics, f"Missing WASM characteristic: {char_uuid}"
    
    print("✅ All WASM characteristics found")
