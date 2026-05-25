import serial
import time
import sys

# Configuration
PORT = '/dev/cu.usbserial-1120' # Update this!
BAUD_RATE = 38400

def force_test():
    print(f"Starting Force Test on {PORT}...")
    try:
        ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
        time.sleep(2) # Wait for reset
        
        # 1. Test Wheels - Forward for 1s
        print("Testing Wheels...")
        ser.write(bytes([2])) # Forward
        time.sleep(1)
        ser.write(bytes([0])) # Stop
        time.sleep(0.5)
        
        # 2. Test Servos - Sweep all
        print("Testing Servos (Sweep)...")
        # Pairs: (Plus, Minus)
        servo_pairs = [(16, 17), (19, 18), (20, 21), (23, 22), (25, 24), (26, 27)]
        
        for plus_cmd, minus_cmd in servo_pairs:
            print(f"Moving Servo pair {plus_cmd}/{minus_cmd}...")
            # Move +
            for _ in range(15):
                ser.write(bytes([plus_cmd]))
                time.sleep(0.05)
            # Move -
            for _ in range(15):
                ser.write(bytes([minus_cmd]))
                time.sleep(0.05)
        
        print("Force Test Sequence Complete.")
        ser.close()
        
    except Exception as e:
        print(f"Error during test: {e}")

if __name__ == "__main__":
    force_test()
