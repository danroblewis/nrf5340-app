#!/usr/bin/env python3
"""
Data Service Tests

Tests for BLE Data Service (0xFFF0) - handles data upload/download operations
"""

import pytest
import asyncio
import logging

logger = logging.getLogger(__name__)

# Service UUIDs
DATA_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"

# Characteristic UUIDs
DATA_UPLOAD_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
DATA_DOWNLOAD_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"


def test_data_service_exists(ble_services):
    """Test that Data Service is discovered"""
    services, characteristics = ble_services
    assert DATA_SERVICE_UUID in services


def test_data_service_characteristics(ble_services):
    """Test that data service characteristics are present"""
    services, characteristics = ble_services
    
    required_chars = [DATA_UPLOAD_UUID, DATA_DOWNLOAD_UUID]
    for char_uuid in required_chars:
        assert char_uuid in characteristics


@pytest.mark.asyncio
async def test_data_service_small_packet(ble_client, ble_services, serial_capture):
    """Test data service with small packet - verify echo functionality"""
    services, characteristics = ble_services
    
    assert DATA_SERVICE_UUID in services
    assert DATA_UPLOAD_UUID in characteristics
    assert DATA_DOWNLOAD_UUID in characteristics
    
    upload_char = characteristics[DATA_UPLOAD_UUID]
    download_char = characteristics[DATA_DOWNLOAD_UUID]
    
    test_data = b"Small test packet"
    
    with serial_capture:
        await ble_client.write_gatt_char(upload_char, test_data)
        await asyncio.sleep(0.1)
        received_data = await ble_client.read_gatt_char(download_char)
    
    # Verify data echo
    assert test_data == received_data
    
    # Verify expected serial output from upload workflow
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    assert "=== Data Service: data_upload_handler called ===" in serial_output
    assert f"Data Service: Upload received {len(test_data)} bytes" in serial_output
    assert "Data Service: Transfer complete" in serial_output
    assert f"Data Service: Saved {len(test_data)} bytes for echo" in serial_output


@pytest.mark.parametrize("packet_size", [16, 32, 64, 128, 200])
@pytest.mark.asyncio
async def test_data_service_large_packets(ble_client, ble_services, serial_capture, packet_size):
    """Test data service with various packet sizes - verify echo for each size"""
    services, characteristics = ble_services
    
    assert DATA_SERVICE_UUID in services
    
    upload_char = characteristics[DATA_UPLOAD_UUID]
    download_char = characteristics[DATA_DOWNLOAD_UUID]
    
    # Use varying pattern data to ensure we can detect corruption
    test_data = bytes([i % 256 for i in range(packet_size)])
    
    with serial_capture:
        await ble_client.write_gatt_char(upload_char, test_data)
        await asyncio.sleep(0.05)
        
        received_data = await ble_client.read_gatt_char(download_char)
        
        # Verify exact echo match
        assert received_data == test_data
    
    # Verify expected serial output for this packet size
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    assert f"Data Service: Upload received {packet_size} bytes" in serial_output
    assert f"Data Service: Saved {packet_size} bytes for echo" in serial_output
    assert "Data Service: Transfer complete" in serial_output


@pytest.mark.asyncio
async def test_data_service_round_trip(ble_client, ble_services, serial_capture):
    """Test complete data service round trip with data integrity verification"""
    services, characteristics = ble_services
    
    assert DATA_SERVICE_UUID in services
    assert DATA_UPLOAD_UUID in characteristics
    assert DATA_DOWNLOAD_UUID in characteristics
    
    test_data = b"Hello from pytest data service test!"
    
    with serial_capture:
        upload_char = characteristics[DATA_UPLOAD_UUID]
        await ble_client.write_gatt_char(upload_char, test_data)
        
        await asyncio.sleep(0.1)
        
        download_char = characteristics[DATA_DOWNLOAD_UUID]
        received_data = await ble_client.read_gatt_char(download_char)
    
    # Verify exact echo match
    assert received_data == test_data
    
    # Verify round-trip workflow in serial output
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    assert "=== Data Service: data_upload_handler called ===" in serial_output
    assert f"Data Service: Upload received {len(test_data)} bytes" in serial_output
    assert "Data Service: Transfer complete" in serial_output
    assert f"Data Service: Saved {len(test_data)} bytes for echo" in serial_output


@pytest.mark.parametrize("test_data,description", [
    (b"Hello World", "simple_text"),
    (b"12345678901234567890", "repeated_pattern"),
    (bytes([i for i in range(32)]), "sequential_bytes"),
    (b"\x00\x01\x02\x03\xff\xfe\xfd\xfc", "mixed_binary"),
    (b"A" * 50, "repeated_character"),
])
@pytest.mark.asyncio
async def test_data_service_packet_processing(ble_client, ble_services, serial_capture, test_data, description):
    """Test that data service processes various data patterns and echoes them correctly"""
    services, characteristics = ble_services
    
    assert DATA_SERVICE_UUID in services
    assert DATA_UPLOAD_UUID in characteristics
    assert DATA_DOWNLOAD_UUID in characteristics
    
    upload_char = characteristics[DATA_UPLOAD_UUID]
    download_char = characteristics[DATA_DOWNLOAD_UUID]
    
    with serial_capture:
        await ble_client.write_gatt_char(upload_char, test_data)
        await asyncio.sleep(0.1)
        received_data = await ble_client.read_gatt_char(download_char)
        
        # Verify exact echo match
        assert received_data == test_data
    
    # Verify data processing workflow in serial output
    serial_result = serial_capture.readouterr()
    serial_output = serial_result.out
    assert "=== Data Service: data_upload_handler called ===" in serial_output
    assert f"Data Service: Upload received {len(test_data)} bytes" in serial_output
    assert "Data Service: Transfer complete" in serial_output
    assert f"Data Service: Saved {len(test_data)} bytes for echo" in serial_output
    # Verify data processing occurred
    assert f"Data Service: Processing {len(test_data)} bytes of data" in serial_output

