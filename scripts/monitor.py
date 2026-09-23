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

    while True:
        try:
            ser = serial.Serial(port, baudrate, timeout=0.1)
            print(f"[Verbunden mit {port}]\n")
            break
        except Exception as e:
            print(f"Warte auf Port {port}... ({e})")
            time.sleep(1.5)
            port = find_arduino_port()

    try:
        while True:
            try:
                line = ser.readline()
                if line:
                    decoded = line.decode('utf-8', errors='replace').rstrip()
                    if decoded:
                        print(decoded)
            except (serial.SerialException, OSError) as err:
                print(f"\n[USB-Verbindung kurz getrennt: {err}]")
                print("[Versuche automatische Wiederverbindung...]")
                try:
                    ser.close()
                except Exception:
                    pass
                time.sleep(1)
                while True:
                    try:
                        port = find_arduino_port()
                        ser = serial.Serial(port, baudrate, timeout=0.1)
                        print(f"[Wieder verbunden mit {port}!]\n")
                        break
                    except Exception:
                        time.sleep(1)
    except KeyboardInterrupt:
        print("\nMonitor beendet.")
    finally:
        try:
            ser.close()
        except Exception:
            pass

if __name__ == '__main__':
    main()
