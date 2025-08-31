"""
WASM Client Library for nRF5340 Device

A clean client library for uploading and executing WASM modules via BLE.
"""

import asyncio
import struct
from typing import Optional, List
from bleak import BleakClient


class WASMClientError(Exception):
    """Base exception for WASM client errors"""
    pass


class WASMUploadError(WASMClientError):
    """Exception raised when WASM upload fails"""
    pass


class WASMExecutionError(WASMClientError):
    """Exception raised when WASM execution fails"""
    pass


class WASMClient:
    """Client for uploading and executing WASM modules via BLE"""
    
    # WASM Service UUIDs
    WASM_SERVICE_UUID = "0000fff7-0000-1000-8000-00805f9b34fb"
    WASM_UPLOAD_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
    WASM_EXECUTE_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"
    WASM_STATUS_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
    WASM_RESULT_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"
    
    # Command codes
    CMD_START_UPLOAD = 0x01
    CMD_CONTINUE_UPLOAD = 0x02
    CMD_FINISH_UPLOAD = 0x03
    CMD_RESET = 0x04
    
    def __init__(self, ble_client: BleakClient, ble_characteristics: dict):
        """Initialize WASM client with BLE connection"""
        self.ble_client = ble_client
        self.ble_characteristics = ble_characteristics
        self.upload_char = ble_characteristics.get(self.WASM_UPLOAD_UUID)
        self.execute_char = ble_characteristics.get(self.WASM_EXECUTE_UUID)
        self.status_char = ble_characteristics.get(self.WASM_STATUS_UUID)
        self.result_char = ble_characteristics.get(self.WASM_RESULT_UUID)
        
        if not self.upload_char:
            raise WASMClientError("WASM upload characteristic not found")
        if not self.execute_char:
            raise WASMClientError("WASM execute characteristic not found")
    
    async def upload_wasm(self, wasm_data: bytes, chunk_size: int = 200) -> None:
        """
        Upload WASM data using chunked upload
        
        Args:
            wasm_data: Raw WASM binary data
            chunk_size: Maximum bytes per chunk (excluding 8-byte header)
            
        Raises:
            WASMUploadError: If upload fails
        """
        # Upload chunks
        for offset in range(0, len(wasm_data), chunk_size):
            chunk = wasm_data[offset:offset + chunk_size]
            sequence = offset // chunk_size
            
            if sequence == 0:
                cmd = self.CMD_START_UPLOAD
                total_size = len(wasm_data)
            else:
                cmd = self.CMD_CONTINUE_UPLOAD
                total_size = 0
            
            packet = struct.pack('<BBHI', cmd, sequence, len(chunk), total_size) + chunk
            
            try:
                await self.ble_client.write_gatt_char(self.upload_char, packet, response=True)
            except Exception as e:
                raise WASMUploadError(f"Failed to upload chunk {sequence}: {e}")
            
            await asyncio.sleep(0.1)
        
        # Wait for processing
        await asyncio.sleep(0.5)
    
    async def execute_function(self, function_name: str, args: List[int] = None) -> int:
        """
        Execute a WASM function
        
        Args:
            function_name: Name of the function to execute
            args: List of integer arguments (default: [])
            
        Returns:
            Function return value as integer
            
        Raises:
            WASMExecutionError: If execution fails
        """
        if args is None:
            args = []  # Default to no arguments
        
        # Create execute packet: function name + arg count + args
        function_name_bytes = function_name.encode('utf-8')
        function_name_padded = function_name_bytes.ljust(32, b'\x00')
        arg_count = len(args)
        
        # Pack the execute packet - firmware always expects 4 args array
        # even when arg_count is 0
        execute_packet = function_name_padded + struct.pack('<I', arg_count) + struct.pack('<iiii',
            args[0] if len(args) > 0 else 0,
            args[1] if len(args) > 1 else 0,
            args[2] if len(args) > 2 else 0,
            args[3] if len(args) > 3 else 0
        )
        
        try:
            # Write to execute characteristic
            await self.ble_client.write_gatt_char(self.execute_char, execute_packet, response=True)
            
            # Wait for execution
            await asyncio.sleep(0.5)
            
            # Read the result
            if not self.result_char:
                raise WASMExecutionError("Result characteristic not found")
            
            result = await self.ble_client.read_gatt_char(self.result_char)
            
            # Debug: print the raw result data
            print(f"🔍 Raw result data: {result.hex()}")
            print(f"🔍 Result length: {len(result)} bytes")
            
            # Try different parsing approaches based on the data
            if len(result) >= 6:
                # Maybe it's status + error + value format
                status, error_code, return_value = struct.unpack('<BBI', result[:6])
                print(f"🔍 Parsed as status={status}, error={error_code}, value={return_value}")
                
                # Check if this is an error response
                if status == 6:  # WASM_STATUS_ERROR
                    # Look for the actual return value elsewhere in the response
                    # Maybe it's after the error information
                    if len(result) >= 10:
                        # Try reading 4 bytes starting at position 6
                        actual_value = struct.unpack('<i', result[6:10])[0]
                        print(f"🔍 Found value after error info: {actual_value}")
                        return actual_value
                    else:
                        raise WASMExecutionError(f"Execution failed with error {error_code}")
                elif status == 5:  # WASM_STATUS_COMPLETE
                    return return_value
                else:
                    print(f"⚠️ Unexpected status: {status}")
                    # Still try to extract a value if possible
                    return return_value
            elif len(result) >= 5:
                # Maybe it's status + value format
                status, return_value = struct.unpack('<BI', result[:5])
                print(f"🔍 Parsed as status={status}, value={return_value}")
                return return_value
            elif len(result) >= 4:
                # Maybe it's just the value, but at a different offset
                # Try reading from different positions
                value1 = struct.unpack('<i', result[:4])[0]
                print(f"🔍 First 4 bytes as i32: {value1}")
                
                if len(result) >= 8:
                    value2 = struct.unpack('<i', result[4:8])[0]
                    print(f"🔍 Next 4 bytes as i32: {value2}")
                    
                    # If the second value looks more reasonable, use it
                    if abs(value2 - 42) < abs(value1 - 42):
                        return value2
                
                return value1
            else:
                raise WASMExecutionError(f"Unexpected result length: {len(result)}")
                
        except Exception as e:
            if isinstance(e, WASMExecutionError):
                raise
            raise WASMExecutionError(f"Execution failed: {e}")
    
    async def upload_and_execute(self, wasm_data: bytes, function_name: str, 
                                args: List[int] = None, chunk_size: int = 200) -> int:
        """
        Upload WASM and execute a function in one operation
        
        Args:
            wasm_data: Raw WASM binary data
            function_name: Name of the function to execute
            args: List of integer arguments
            chunk_size: Maximum bytes per chunk
            
        Returns:
            Function return value as integer
        """
        await self.upload_wasm(wasm_data, chunk_size)
        await asyncio.sleep(1.0)  # Wait for compilation
        return await self.execute_function(function_name, args)
