#!/usr/bin/env python3
"""
BLE Connection Tests

General BLE connection and discovery tests
"""

import pytest
import asyncio

# All expected service UUIDs
EXPECTED_SERVICES = [
    "0000180a-0000-1000-8000-00805f9b34fb",  # Device Information
    "0000ffe0-0000-1000-8000-00805f9b34fb",  # Control Service
    "0000fff0-0000-1000-8000-00805f9b34fb",  # Data Service
    "0000fe59-0000-1000-8000-00805f9b34fb",  # DFU Service
    "0000fff8-0000-1000-8000-00805f9b34fb",  # Sprite Service
    "0000fff7-0000-1000-8000-00805f9b34fb",  # WASM Service
]


def test_ble_connection_established(ble_client):
    """Test that BLE connection is established"""
    assert ble_client is not None
    assert ble_client.is_connected


def test_ble_connection_properties(ble_client):
    """Test BLE connection properties"""
    assert ble_client.is_connected
    assert ble_client.mtu_size >= 23  # Minimum MTU
    assert ble_client.mtu_size <= 517  # Maximum MTU


def test_all_services_discovered(ble_services):
    """Test that all expected services are discovered"""
    services, characteristics = ble_services
    
    for service_uuid in EXPECTED_SERVICES:
        assert service_uuid in services


@pytest.mark.asyncio
async def test_mtu_negotiation_results(ble_client):
    """Test that MTU negotiation worked properly"""
    mtu = ble_client.mtu_size
    
    # Expect reasonable MTU size for modern BLE
    if mtu == 23:
        # Default minimum MTU
        assert True
    else:
        # Allow for slightly different negotiated values
        assert mtu > 23 and mtu <= 517


@pytest.mark.asyncio
async def test_concurrent_service_access(ble_client, ble_services, serial_capture):
    """Test accessing multiple services concurrently"""
    services, characteristics = ble_services
    
    # Service UUIDs
    DEVICE_INFO_SERVICE_UUID = "0000180a-0000-1000-8000-00805f9b34fb"
    DATA_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"
    
    # Characteristic UUIDs
    MANUFACTURER_NAME_UUID = "00002a29-0000-1000-8000-00805f9b34fb"
    DATA_UPLOAD_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
    DATA_DOWNLOAD_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"
    
    with serial_capture:
        tasks = []
        
        # Read device info
        if MANUFACTURER_NAME_UUID in characteristics:
            char = characteristics[MANUFACTURER_NAME_UUID]
            tasks.append(ble_client.read_gatt_char(char))
        
        # Test data service
        if DATA_UPLOAD_UUID in characteristics and DATA_DOWNLOAD_UUID in characteristics:
            upload_char = characteristics[DATA_UPLOAD_UUID]
            download_char = characteristics[DATA_DOWNLOAD_UUID]
            
            async def data_test():
                await ble_client.write_gatt_char(upload_char, b"concurrent test")
                await asyncio.sleep(0.05)
                return await ble_client.read_gatt_char(download_char)
            
            tasks.append(data_test())
        
        # Execute concurrently
        if tasks:
            results = await asyncio.gather(*tasks, return_exceptions=True)
            
            # All operations should succeed (no exceptions)
            for result in results:
                assert not isinstance(result, Exception)


def test_service_uuids_format(ble_services):
    """Test that service UUIDs are properly formatted"""
    services, characteristics = ble_services
    
    for service_uuid in services:
        # Standard UUID format: 8-4-4-4-12 characters
        assert len(service_uuid) == 36
        assert service_uuid.count('-') == 4


def test_characteristic_count_reasonable(ble_services):
    """Test that we have a reasonable number of characteristics"""
    services, characteristics = ble_services
    
    # Should have at least 10 characteristics across all services
    assert len(characteristics) >= 10
    
    # But not an unreasonable number (< 100)
    assert len(characteristics) < 100
