import tkinter as tk
import serial
import time
import threading

# Configuration
PORT = '/dev/cu.usbserial-120'
BAUD = 38400

class RealtimeController:
    def __init__(self, root):
        self.root = root
        self.root.title("xDots Realtime Dashboard")
        self.root.geometry("400x500")
        self.root.configure(bg='#0a0a0b')

        self.ser = None
        self.active_keys = set()
        self.last_sent = -1

        # Command Map
        self.COMMANDS = {
            'w': 2, 's': 7, 'a': 4, 'd': 5,
            'q': 1, 'e': 3, 'z': 6, 'c': 8,
            'j': 9, 'l': 10,
            '1': 16, '!': 17, '2': 19, '@': 18,
            '3': 20, '#': 21, '4': 23, '$': 22,
            '5': 25, '%': 24, '6': 26, '^': 27
        }

        self.setup_ui()
        self.connect_serial()
        
        # Start the pulse loop (Realtime heartbeat)
        self.pulse()

        # Bind events
        self.root.bind("<KeyPress>", self.on_press)
        self.root.bind("<KeyRelease>", self.on_release)

    def setup_ui(self):
        self.header = tk.Label(self.root, text="xDots Robotix", font=("Helvetica", 20, "bold"), fg="#00f2ff", bg="#0a0a0b")
        self.header.pack(pady=20)

        self.status_label = tk.Label(self.root, text="STATUS: CONNECTING...", font=("Helvetica", 10), fg="#ff0055", bg="#0a0a0b")
        self.status_label.pack()

        self.cmd_display = tk.Label(self.root, text="IDLE", font=("Courier", 30, "bold"), fg="#fff", bg="#1a1a1b", width=10, height=2)
        self.cmd_display.pack(pady=30)

        self.help = tk.Label(self.root, text="W/A/S/D: Move\n1-6: Servos (+)\nShift+1-6: Servos (-)", font=("Helvetica", 10), fg="#666", bg="#0a0a0b")
        self.help.pack(side="bottom", pady=20)

    def connect_serial(self):
        try:
            self.ser = serial.Serial(PORT, BAUD, timeout=0.1)
            time.sleep(2)
            self.status_label.config(text=f"CONNECTED TO {PORT}", fg="#00f2ff")
        except Exception as e:
            self.status_label.config(text=f"OFFLINE: {e}", fg="#ff0055")

    def on_press(self, event):
        key = event.char.lower()
        if key in self.COMMANDS:
            self.active_keys.add(key)

    def on_release(self, event):
        key = event.char.lower()
        if key in self.active_keys:
            self.active_keys.remove(key)
        if not self.active_keys:
            self.send_command(0)

    def send_command(self, cmd):
        if self.ser and self.ser.is_open:
            self.ser.write(bytes([cmd]))
            self.cmd_display.config(text=str(cmd) if cmd != 0 else "STOP")
            if cmd != 0: self.cmd_display.config(fg="#00f2ff")
            else: self.cmd_display.config(fg="#ff0055")

    def pulse(self):
        """Main loop that sends the active command repeatedly while key is held."""
        if self.active_keys:
            # For simplicity, we take the most recent key
            key = list(self.active_keys)[-1]
            cmd = self.COMMANDS[key]
            self.send_command(cmd)
        
        self.root.after(50, self.pulse) # 20Hz update rate

if __name__ == "__main__":
    root = tk.Tk()
    app = RealtimeController(root)
    root.mainloop()
