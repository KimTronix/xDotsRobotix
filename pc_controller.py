import serial
import serial.tools.list_ports
import time
import sys
import termios
import tty

# Configuration - update the port to match your system
# On Mac it's usually /dev/cu.usbmodemXXXX or /dev/tty.usbmodemXXXX
PORT = '/dev/cu.usbserial-120' 
BAUD_RATE = 38400

# Command Mapping
COMMANDS = {
    'w': 2,   # Forward
    's': 7,   # Backward
    'a': 4,   # Sideways Left
    'd': 5,   # Sideways Right
    'q': 1,   # Left Forward
    'e': 3,   # Right Forward
    'z': 6,   # Left Backward
    'c': 8,   # Right Backward
    'j': 9,   # Rotate Left
    'l': 10,  # Rotate Right
    ' ': 0,   # Stop
    'x': 0,   # Stop
    
    # Servos (Testing mode - simple toggle/step)
    '1': 16,  # S1 +
    '!': 17,  # S1 - (Shift+1)
    '2': 19,  # S2 +
    '@': 18,  # S2 - (Shift+2)
    '3': 20,  # S3 +
    '#': 21,  # S3 - (Shift+3)
    '4': 23,  # S4 +
    '$': 22,  # S4 - (Shift+4)
    '5': 25,  # S5 +
    '%': 24,  # S5 - (Shift+5)
    '6': 26,  # S6 +
    '^': 27,  # S6 - (Shift+6)
    
    # Logic
    'v': 12,  # Save Step
    'r': 14,  # Run Steps
    't': 'test', # Self-Test
}

def run_self_test(ser):
    print("\nRunning Self-Test Sequence...")
    # Stop everything first
    ser.write(bytes([0]))
    time.sleep(0.5)
    
    # Test Servos
    print("Testing Servos (Sweep)...")
    servo_pairs = [(16, 17), (19, 18), (20, 21), (23, 22), (25, 24), (26, 27)]
    for plus_cmd, minus_cmd in servo_pairs:
        print(f"Testing Servo Pair {plus_cmd}/{minus_cmd}...")
        for _ in range(10):
            ser.write(bytes([plus_cmd]))
            time.sleep(0.05)
        for _ in range(10):
            ser.write(bytes([minus_cmd]))
            time.sleep(0.05)
    
    # Test Wheels
    print("Testing Wheels Forward...")
    ser.write(bytes([2]))
    time.sleep(1)
    ser.write(bytes([0]))
    
    print("Self-Test Complete.")

def getch():
    """Read a single character from stdin. Fallback to input() if not in a TTY."""
    try:
        import termios
        import tty
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch
    except Exception:
        # Fallback for non-interactive terminals or IDEs
        print("\r[Input needed + Enter]: ", end="")
        return sys.stdin.read(1)

def main():
    print("--- xDots Robotix PC/Python Controller ---")
    print(f"Connecting to {PORT} at {BAUD_RATE} baud...")
    
    try:
        ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
        time.sleep(2) # Wait for Arduino to reset
        print("Connected!")
    except Exception as e:
        print(f"Error connecting to {PORT}: {e}")
        print("\nAvailable ports:")
        ports = serial.tools.list_ports.comports()
        for p in ports:
            print(f"  - {p.device} ({p.description})")
        print("\nTIP: Update the 'PORT' variable in this script with one of the above.")
        return

    print("\nControls:")
    print("  W/S/A/D: Move (Fwd/Bwd/Left/Right)")
    print("  Q/E/Z/C: Diagonals")
    print("  J/L: Rotate")
    print("  Space/X: Stop")
    print("  1-6: Adjust Servos (+)")
    print("  Shift+1-6 (!@#$%^): Adjust Servos (-)")
    print("  V: Save Step")
    print("  R: Run Recorded Steps")
    print("  T: Run Self-Test Sequence")
    print("  Ctrl+C: Exit")

    try:
        while True:
            char = getch()
            if char == '\x03': # Ctrl+C
                break
            
            if char.lower() == 't':
                run_self_test(ser)
                continue
            
            if char.lower() in COMMANDS:
                cmd_byte = COMMANDS[char.lower()]
                ser.write(bytes([cmd_byte]))
                print(f"Sent Command: {char} ({cmd_byte})", end='\r')
            elif char in COMMANDS: # For shifted characters (!@#...)
                cmd_byte = COMMANDS[char]
                ser.write(bytes([cmd_byte]))
                print(f"Sent Command: {char} ({cmd_byte})", end='\r')
                
    except KeyboardInterrupt:
        pass
    finally:
        if 'ser' in locals():
            ser.close()
        print("\nDisconnected.")

if __name__ == "__main__":
    main()
