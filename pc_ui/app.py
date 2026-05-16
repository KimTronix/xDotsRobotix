from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import serial
import serial.tools.list_ports
import time

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

PORT = '/dev/cu.usbserial-120'
BAUD_RATE = 115200
ser = None

def find_arduino():
    for p in serial.tools.list_ports.comports():
        if 'usbserial' in p.device.lower() or 'usbmodem' in p.device.lower():
            return p.device
    return None

def connect():
    global ser, PORT
    if ser is not None and ser.is_open:
        return ser
    try:
        target = PORT if PORT else find_arduino()
        if not target:
            return None
        ser = serial.Serial(target, BAUD_RATE, timeout=0.1)
        time.sleep(2)
        PORT = target
        socketio.emit('serial_rx', {'line': f'[NEXUS] Connected to {target}'})
        socketio.emit('status', {'connected': True, 'port': target})
        return ser
    except Exception as e:
        ser = None
        socketio.emit('serial_rx', {'line': f'[ERROR] {e}'})
        socketio.emit('status', {'connected': False, 'port': PORT})
        return None

def serial_reader():
    """Continuously read from Arduino and push to UI."""
    while True:
        try:
            s = connect()
            if s and s.is_open:
                line = s.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    socketio.emit('serial_rx', {'line': line})
            else:
                time.sleep(2)
        except Exception as e:
            global ser
            ser = None
            socketio.emit('serial_rx', {'line': f'[LOST] {e}'})
            socketio.emit('status', {'connected': False, 'port': PORT})
            time.sleep(2)

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def on_connect():
    s = connect()
    emit('status', {'connected': s is not None, 'port': PORT})

@socketio.on('command')
def handle_command(data):
    global ser
    cmd = data.get('cmd')
    if cmd is None:
        return
    s = connect()
    if s:
        try:
            s.write(bytes([int(cmd)]))
        except Exception as e:
            ser = None
            emit('serial_rx', {'line': f'[TX ERROR] {e}'})

@socketio.on('get_ports')
def handle_get_ports():
    ports = [p.device for p in serial.tools.list_ports.comports()]
    emit('ports_list', {'ports': ports})

@socketio.on('set_port')
def handle_set_port(data):
    global PORT, ser
    new_port = data.get('port')
    if new_port:
        if ser:
            try: ser.close()
            except: pass
        ser = None
        PORT = new_port
        s = connect()
        emit('status', {'connected': s is not None, 'port': PORT})

@socketio.on('reset_ports')
def handle_reset_ports():
    import os
    global ser
    if ser:
        try: ser.close()
        except: pass
        ser = None
    os.system("pkill -f serial-monitor")
    os.system("pkill -f serial-mo")
    emit('serial_rx', {'line': '[SYSTEM] Background serial processes terminated.'})

import json
import os

SEQUENCES_FILE = 'sequences.json'

def load_sequences():
    if os.path.exists(SEQUENCES_FILE):
        try:
            with open(SEQUENCES_FILE, 'r') as f:
                return json.load(f)
        except: pass
    return {}

@socketio.on('save_sequence')
def handle_save_sequence(data):
    name = data.get('name')
    seq = data.get('sequence')
    if name and seq:
        seqs = load_sequences()
        seqs[name] = seq
        with open(SEQUENCES_FILE, 'w') as f:
            json.dump(seqs, f)
        emit('sequences_list', {'sequences': list(seqs.keys())}, broadcast=True)
        emit('serial_rx', {'line': f'[SYSTEM] Sequence "{name}" saved.'})

@socketio.on('get_sequences')
def handle_get_sequences():
    seqs = load_sequences()
    emit('sequences_list', {'sequences': list(seqs.keys())})

playing_sequence = False

@socketio.on('play_sequence')
def handle_play_sequence(data):
    global playing_sequence
    name = data.get('name')
    loops = int(data.get('loops', -1))
    if not name: return
    seqs = load_sequences()
    if name in seqs:
        seq = seqs[name]
        playing_sequence = True
        socketio.start_background_task(play_task, seq, loops, name)

@socketio.on('stop_sequence')
def handle_stop_sequence():
    global playing_sequence
    playing_sequence = False
    emit('serial_rx', {'line': '[SYSTEM] Halting playback...'})

def play_task(seq, loops, name):
    global playing_sequence
    count = 0
    while playing_sequence and (loops <= 0 or count < loops):
        s = connect()
        if not s: break
        
        # 1. Return to Home Position
        socketio.emit('serial_rx', {'line': f'[SYSTEM] Returning to HOME (Loop {count+1})...'})
        try: s.write(bytes([13]))
        except: pass
        
        # Wait 4 seconds for arm to physically reach home
        for _ in range(40):
            if not playing_sequence: break
            time.sleep(0.1)
            
        if not playing_sequence: break

        # 2. Play the Sequence
        socketio.emit('serial_rx', {'line': f'[SYSTEM] Executing sequence "{name}"...'})
        for step in seq:
            if not playing_sequence: break
            delay = step.get('delay', 0) / 1000.0
            if delay > 0:
                # Sleep in small chunks to remain responsive to stop signal
                chunks = int(delay / 0.1)
                for _ in range(chunks):
                    if not playing_sequence: break
                    time.sleep(0.1)
                if playing_sequence:
                    time.sleep(delay - (chunks * 0.1))
            
            if not playing_sequence: break
            cmd = step.get('cmd')
            if cmd is not None:
                try: s.write(bytes([int(cmd)]))
                except: pass
        
        count += 1

    # 3. Final Return to Home
    if s and playing_sequence:
        socketio.emit('serial_rx', {'line': '[SYSTEM] Sequence complete. Returning HOME...'})
        try: s.write(bytes([13]))
        except: pass

    playing_sequence = False
    socketio.emit('serial_rx', {'line': '[SYSTEM] Playback stopped.'})


if __name__ == '__main__':
    import threading
    t = threading.Thread(target=serial_reader, daemon=True)
    t.start()
    print("--- xDots NEXUS ---")
    print("Dashboard: http://127.0.0.1:5050")
    socketio.run(app, debug=False, host='0.0.0.0', port=5050, use_reloader=False)
