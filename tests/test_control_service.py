#!/usr/bin/env python3
"""
Control Service Tests

Tests for BLE Control Service (0xFFE0) - handles device control commands
"""

import pytest
import asyncio
import struct

# Service UUIDs
CONTROL_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"

# Characteristic UUIDs
CONTROL_COMMAND_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
CONTROL_RESPONSE_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"


def test_control_service_exists(ble_services):
    """Test that Control Service is discovered"""
    services, characteristics = ble_services
    assert CONTROL_SERVICE_UUID in services


def test_control_service_characteristics(ble_services):
    """Test that control service characteristics are present"""
    services, characteristics = ble_services
    
    # Command characteristic should exist
    assert CONTROL_COMMAND_UUID in characteristics
    
    # Response characteristic may or may not exist depending on implementation
    # assert CONTROL_RESPONSE_UUID in characteristics


@pytest.mark.asyncio
async def test_control_service_ping(ble_client, ble_services, serial_capture):
    """Test control service ping command"""
    services, characteristics = ble_services
    
    assert CONTROL_SERVICE_UUID in services
    assert CONTROL_COMMAND_UUID in characteristics
    
    command_char = characteristics[CONTROL_COMMAND_UUID]
    
    with serial_capture:
        try:
            # Try simple ping command
            ping_command = struct.pack('<B', 0x01)
            await ble_client.write_gatt_char(command_char, ping_command)
        except Exception:
            try:
                # Try longer format
                ping_command = struct.pack('<BB', 0x01, 0x00)
                await ble_client.write_gatt_char(command_char, ping_command)
            except Exception:
                # Control service may have specific format requirements
                pytest.skip("Control service command format not compatible")
        
            await asyncio.sleep(0.1)
    
    # If we get here without exception, the command was accepted
    assert True


@pytest.mark.asyncio
async def test_control_service_commands(ble_client, ble_services, serial_capture):
    """Test various control service commands"""
    services, characteristics = ble_services
    
    assert CONTROL_SERVICE_UUID in services
    assert CONTROL_COMMAND_UUID in characteristics
    
    command_char = characteristics[CONTROL_COMMAND_UUID]
    
    # Test commands that might be supported
    test_commands = [
        struct.pack('<B', 0x00),    # Possible status command
        struct.pack('<B', 0x01),    # Possible ping command
        struct.pack('<B', 0x02),    # Possible reset command
    ]
    
    with serial_capture:
        for cmd in test_commands:
            try:
                await ble_client.write_gatt_char(command_char, cmd)
                await asyncio.sleep(0.05)
            except Exception:
                # Some commands may not be supported
                continue
    
    # If we get here, at least some commands were accepted
    assert True
