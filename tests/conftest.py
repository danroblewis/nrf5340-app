#!/usr/bin/env python3
"""
Simple Pytest Configuration

Provides basic BLE connection and serial monitoring fixtures without 
competing with pytest's built-in assertion and testing mechanisms.
"""

import pytest
import pytest_asyncio
import asyncio
import logging
import time
import serial
import threading
from pathlib import Path
from bleak import BleakClient, BleakScanner
from typing import List, Optional
from collections import namedtuple

# Test configuration
DEVICE_NAME = "Dan5340BLE"
SERIAL_PORT = "/dev/tty.usbmodem0010500306563"
SERIAL_BAUD = 115200
CONNECTION_TIMEOUT = 15.0
DISCOVERY_TIMEOUT = 10.0

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(levelname)s:%(name)s:%(message)s')
logger = logging.getLogger(__name__)

# Serial capture result
SerialResult = namedtuple('SerialResult', ['out', 'err'])

class SerialCapture:
    """
    Serial capture utility similar to pytest's capsys.
    
    Usage in tests:
        def test_something(serial_capture):
            with serial_capture:
                # perform operations that generate serial output
                pass
            result = serial_capture.readouterr()
            assert "expected text" in result.out
    """
    
    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self.serial = None
        self.captured_lines = []
        self.capturing = False
        self._capture_thread = None
        self._stop_event = None
    
    def __enter__(self):
        """Start capturing serial data"""
        self.start_capture()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Stop capturing serial data"""
        self.stop_capture()
    
    def start_capture(self):
        """Start capturing serial data in background thread"""
        if self.capturing:
            return
        
        try:
            self.serial = serial.Serial(self.port, self.baud, timeout=0.1)
            self.captured_lines = []
            self.capturing = True
            self._stop_event = threading.Event()
            
            # Start capture thread
            self._capture_thread = threading.Thread(target=self._capture_loop, daemon=True)
            self._capture_thread.start()
            
            # Brief delay to start capturing
            time.sleep(0.1)
            logger.debug(f"✅ Serial capture started on {self.port}")
            
        except Exception as e:
            logger.warning(f"⚠️ Failed to start serial capture: {e}")
            self.capturing = False
    
    def stop_capture(self):
        """Stop capturing serial data"""
        if not self.capturing:
            return
        
        self.capturing = False
        
        if self._stop_event:
            self._stop_event.set()
        
        if self._capture_thread and self._capture_thread.is_alive():
            self._capture_thread.join(timeout=1.0)
        
        if self.serial and self.serial.is_open:
            self.serial.close()
        
        logger.debug("📡 Serial capture stopped")
    
    def _capture_loop(self):
        """Background thread loop to capture serial data"""
        while self.capturing and not self._stop_event.is_set():
            try:
                if self.serial and self.serial.is_open and self.serial.in_waiting > 0:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.captured_lines.append(line)
                else:
                    time.sleep(0.01)  # Small delay to prevent busy loop
            except Exception as e:
                logger.warning(f"Serial capture error: {e}")
                break
    
    def readouterr(self) -> SerialResult:
        """
        Read and return captured serial output.
        Similar to capsys.readouterr()
        
        Returns:
            SerialResult with 'out' field containing captured lines
        """
        # Give a moment for any final data
        if self.capturing:
            time.sleep(0.05)
        
        captured = '\n'.join(self.captured_lines)
        self.captured_lines = []  # Clear for next capture
        
        return SerialResult(out=captured, err='')
    
    def get_lines(self) -> List[str]:
        """Get captured lines as a list"""
        return self.captured_lines.copy()
    
    def clear(self):
        """Clear captured data"""
        self.captured_lines = []

@pytest_asyncio.fixture(scope="function")
async def ble_setup():
    """Setup BLE connection for each test function"""
    logger.info("🚀 Setting up BLE connection...")
    
    # Discover device
    logger.info(f"🔍 Scanning for BLE device: {DEVICE_NAME}...")
    devices = await BleakScanner.discover(timeout=DISCOVERY_TIMEOUT)
    device = None
    
    for d in devices:
        if d.name == DEVICE_NAME:
            device = d
            logger.info(f"📱 Found device: {d.name} ({d.address})")
            break
    
    if not device:
        pytest.skip(f"BLE device '{DEVICE_NAME}' not found")
    
    # Connect to device
    logger.info(f"🔗 Connecting to {device.address}...")
    client = BleakClient(device.address)
    await client.connect(timeout=CONNECTION_TIMEOUT)
    
    if not client.is_connected:
        pytest.skip("Failed to connect to BLE device")
    
    logger.info(f"✅ Connected to {DEVICE_NAME}")
    logger.info(f"📏 MTU: {client.mtu_size} bytes")
    
    # Discover services
    logger.info("🔍 Discovering BLE services...")
    services = {}
    characteristics = {}
    
    for service in client.services:
        services[service.uuid.lower()] = service
        for char in service.characteristics:
            characteristics[char.uuid.lower()] = char
    
    logger.info(f"✅ Discovered {len(services)} services, {len(characteristics)} characteristics")
    
    yield {
        'client': client,
        'services': services,
        'characteristics': characteristics
    }
    
    # Cleanup
    logger.info("🧹 Cleaning up BLE connection...")
    if client.is_connected:
        await client.disconnect()

@pytest.fixture
def ble_client(ble_setup):
    """Get BLE client"""
    return ble_setup['client']

@pytest.fixture  
def ble_services(ble_setup):
    """Get BLE services and characteristics"""
    return ble_setup['services'], ble_setup['characteristics']

@pytest.fixture
def serial_capture():
    """
    Provides serial capture functionality similar to capsys.
    
    Usage:
        def test_something(serial_capture):
            with serial_capture:
                # operations that generate serial output
                await ble_client.write_gatt_char(char, data)
            
            result = serial_capture.readouterr()
            assert "WASM uploaded" in result.out
    """
    if not Path(SERIAL_PORT).exists():
        logger.warning(f"⚠️ Serial port {SERIAL_PORT} not found")
        return None
    
    capture = SerialCapture(SERIAL_PORT, SERIAL_BAUD)
    
    yield capture
    
    # Cleanup
    capture.stop_capture()

# Pytest hooks for setup/teardown
def pytest_configure(config):
    """Pytest configuration hook"""
    config.addinivalue_line("markers", "ble: BLE tests")
    config.addinivalue_line("markers", "wasm: WASM service tests")
    config.addinivalue_line("markers", "sprite: Sprite service tests") 
    config.addinivalue_line("markers", "slow: Slow running tests")
    config.addinivalue_line("markers", "serial: Tests that require serial monitoring")