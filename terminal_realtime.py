import sys
import tty
import termios
import serial
import time

# Configuration
PORT = '/dev/cu.usbserial-120'
BAUD = 38400

# Command Map
COMMANDS = {
    'w': 2, 's': 7, 'a': 4, 'd': 5,
    'x': 0, ' ': 0,
    '1': 16, '2': 19, '3': 20, '4': 23, '5': 25, '6': 26
}

def run_terminal_realtime():
    print(f"Connecting to {PORT}...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.1)
        time.sleep(2)
        print("Connected! DRIVE MODE ACTIVE (No Enter needed).")
        print("Use W/A/S/D to move, 1-6 for Servos. Press 'Q' to Quit.")
    except Exception as e:
        print(f"Error: {e}")
        return

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    
    try:
        # setraw is better for "no echo" realtime feel
        tty.setraw(fd)
        while True:
            char = sys.stdin.read(1).lower()
            if char == 'q' or char == '\x03': # Q or Ctrl+C
                break
            
            if char in COMMANDS:
                cmd = COMMANDS[char]
                ser.write(bytes([cmd]))
                # For servos, we send it, then a tiny delay to make it feel "realtime"
                if cmd >= 16:
                    time.sleep(0.05)
            
    except Exception as e:
        pass # Handle exit gracefully
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        ser.close()
        print("\r\n[SYSTEM] Terminal restored. Robot connection closed.")

if __name__ == "__main__":
    run_terminal_realtime()
