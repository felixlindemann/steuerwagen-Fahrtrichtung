#!/usr/bin/env python3
"""
Einfacher serieller Monitor für den Arduino Nano.
Liest Ausgaben bei 115200 Baud und zeigt Richtungswechsel in Echtzeit an.
Beenden mit Ctrl+C.
"""

import sys
import time
import glob

try:
    import serial
except ImportError:
    print("Fehler: 'pyserial' ist nicht installiert. Bitte 'pip3 install pyserial' ausführen.")
    sys.exit(1)

def find_arduino_port():
    candidates = glob.glob('/dev/cu.usbserial*') + glob.glob('/dev/cu.wchusbserial*')
    if candidates:
        return candidates[0]
    return '/dev/cu.usbserial-143320'

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_arduino_port()
    baudrate = 115200

    print("========================================================")
    print(f" Verbinde mit Arduino Nano an {port} ({baudrate} Baud)...")
    print(" Beenden mit Ctrl + C")
    print("========================================================\n")

    try:
        ser = serial.Serial(port, baudrate, timeout=0.1)
    except Exception as e:
        print(f"Konnte Port {port} nicht öffnen: {e}")
        sys.exit(1)

    try:
        while True:
            line = ser.readline()
            if line:
                decoded = line.decode('utf-8', errors='replace').rstrip()
                if decoded:
                    print(decoded)
    except KeyboardInterrupt:
        print("\nMonitor beendet.")
    finally:
        ser.close()

if __name__ == '__main__':
    main()
