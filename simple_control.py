import serial
import time
import sys

# Update this to your port!
PORT = '/dev/cu.usbserial-120'
BAUD = 115200 # Updated to match simplified firmware

def run_simple():
    print(f"--- xDots Simple Controller ---")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        time.sleep(2)
        print("Robot Connected!")
    except Exception as e:
        print(f"Error: {e}")
        return

    print("\nControls (Type key + Enter):")
    print("  W/A/S/D: Move")
    print("  1-6: Move Servos")
    print("  B/N: Gripper Close/Open")
    print("  X: Stop")
    print("  Q: Quit")

    COMMANDS = {
        'w': 2, 's': 7, 'a': 4, 'd': 5, 'x': 0,
        '1': 16, '2': 19, '3': 20, '4': 23, '5': 25, '6': 26,
        'b': 26, 'n': 27
    }

    try:
        while True:
            char = sys.stdin.read(1).lower()
            if char == 'q': break
            if char in COMMANDS:
                ser.write(bytes([COMMANDS[char]]))
                print(f"-> Sent {char}", end='\r')
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("\nDisconnected.")

if __name__ == "__main__":
    run_simple()
