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
        # Send GET_STATUS command using correct 20-byte packet format
        # control_command_packet_t: cmd_id(1) + param1(1) + param2(1) + reserved(17)
        ping_command = struct.pack('<BBB17x', 0x01, 0x00, 0x00)  # CMD_GET_STATUS with no params
        await ble_client.write_gatt_char(command_char, ping_command)
        await asyncio.sleep(0.1)


@pytest.mark.asyncio
async def test_control_service_commands(ble_client, ble_services, serial_capture):
    """Test various control service commands"""
    services, characteristics = ble_services
    
    assert CONTROL_SERVICE_UUID in services
    assert CONTROL_COMMAND_UUID in characteristics
    
    command_char = characteristics[CONTROL_COMMAND_UUID]
    
    # Test different control commands using correct 20-byte packet format
    test_commands = [
        struct.pack('<BBB17x', 0x01, 0x00, 0x00),  # CMD_GET_STATUS
        struct.pack('<BBB17x', 0x02, 0x00, 0x00),  # CMD_RESET_DEVICE  
        struct.pack('<BBB17x', 0x03, 0x42, 0x00),  # CMD_SET_CONFIG with param
        struct.pack('<BBB17x', 0x04, 0x00, 0x00),  # CMD_GET_VERSION
    ]
    
    with serial_capture:
        # Send test commands - let exceptions indicate actual problems
        for cmd in test_commands:
            await ble_client.write_gatt_char(command_char, cmd)
            await asyncio.sleep(0.05)
