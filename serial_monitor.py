#!/usr/bin/env python3
"""
Serial Monitor Script

Connects to a serial device and monitors incoming data, printing it unbuffered
until the connection is closed or interrupted.
"""

import serial
import sys
import signal
import time

# Configuration
DEVICE_PATH = "/dev/tty.usbmodem0010500306563"
BAUD_RATE = 115200
TIMEOUT = 0.1  # Non-blocking read timeout

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    print("\nExiting serial monitor...")
    sys.exit(0)

def main():
    # Register signal handler for graceful exit
    signal.signal(signal.SIGINT, signal_handler)
    
    try:
        # Open serial connection
        print(f"Connecting to {DEVICE_PATH} at {BAUD_RATE} baud...")
        ser = serial.Serial(
            port=DEVICE_PATH,
            baudrate=BAUD_RATE,
            timeout=TIMEOUT,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS
        )
        
        # Flush any existing data
        ser.flushInput()
        ser.flushOutput()
        
        print(f"Connected! Monitoring serial data (Press Ctrl+C to exit)...")
        print("-" * 50)
        
        # Monitor serial data
        while True:
            try:
                # Read data from serial port
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    if data:
                        # Decode and print without buffering
                        try:
                            text = data.decode('utf-8', errors='replace')
                            print(text, end='', flush=True)
                        except UnicodeDecodeError:
                            # If decoding fails, print raw bytes
                            print(f"[RAW: {data.hex()}]", flush=True)
                else:
                    # Small sleep to prevent excessive CPU usage
                    time.sleep(0.01)
                    
            except serial.SerialException as e:
                print(f"\nSerial error: {e}")
                break
                
    except serial.SerialException as e:
        print(f"Failed to connect to {DEVICE_PATH}: {e}")
        print("Make sure the device is connected and the path is correct.")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nExiting...")
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)
    finally:
        # Clean up
        try:
            if 'ser' in locals() and ser.is_open:
                ser.close()
                print("Serial connection closed.")
        except:
            pass

if __name__ == "__main__":
    main()
