#!/bin/bash

# BLE Testing Script Runner for nRF5340
# This script runs comprehensive BLE service tests

echo "🔵 nRF5340 BLE Testing Suite"
echo "============================"
echo ""

# Check if device is likely connected/flashed
echo "📋 Pre-flight checks:"
echo "  - Make sure your nRF5340 is connected and flashed"
echo "  - Make sure Bluetooth is enabled on this computer"
echo "  - Make sure the device is advertising (should show up in BLE scanner apps)"
echo ""

# Check Python dependencies
echo "🐍 Checking Python dependencies..."
python3 -c "import bleak; print('✅ bleak is available')" 2>/dev/null || {
    echo "❌ bleak not installed"
    echo "   Install with: pip install bleak"
    exit 1
}

python3 -c "import asyncio; print('✅ asyncio is available')" 2>/dev/null || {
    echo "❌ asyncio not available"
    echo "   This usually comes with Python 3.7+"
    exit 1
}

python3 -c "import struct; print('✅ struct is available')" 2>/dev/null || {
    echo "❌ struct not available"
    echo "   This should come with Python standard library"
    exit 1
}

echo ""

# Run comprehensive tests
echo "🧪 Running comprehensive BLE tests..."
echo "====================================="
echo ""
python3 test_ble_comprehensive.py
RESULT=$?

echo ""
echo ""

# Summary
echo "📊 Final Summary"
echo "================"

if [ $RESULT -eq 0 ]; then
    echo "🎉 ALL BLE TESTS PASSED!"
    echo ""
    echo "✅ Device Information Service: Working"
    echo "✅ Control Service: Working"  
    echo "✅ Data Service: Working (Round-trip verified)"
    echo "✅ DFU Service: Working"
    echo ""
    echo "Your nRF5340 BLE services are fully functional!"
    echo ""
    echo "Available individual test files:"
    echo "  - test_ble_comprehensive.py  (All tests combined)"
    echo "  - test_data_roundtrip.py     (Data Service specific round-trip test)"
    echo "  - test_ble_device.py         (Device Info Service only)"
    echo "  - test_custom_services.py    (Custom services template)"
    exit 0
else
    echo "❌ Some tests failed - check the detailed output above"
    echo ""
    echo "Troubleshooting tips:"
    echo "  - Check device connection and ensure it's flashed with latest firmware"
    echo "  - Verify device is advertising (should appear in BLE scanner apps)"
    echo "  - Check serial console for device-side error messages"
    echo "  - Ensure no other applications are connected to the device"
    exit 1
fi
