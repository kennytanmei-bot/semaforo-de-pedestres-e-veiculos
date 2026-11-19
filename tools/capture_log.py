#!/usr/bin/env python3
"""Captura o log serial de uma placa para arquivo (Windows/Linux/Mac).

  python tools/capture_log.py COM5 a.log [--seconds 120]
Ctrl+C encerra. Requer pyserial (ja vem com o PlatformIO; senao: pip install pyserial).
Descubra a porta com:  pio device list
Feche o 'pio device monitor' antes (a porta so aceita um programa).
"""
import argparse, sys, time
try:
    import serial
except ImportError:
    sys.exit("Instale pyserial: pip install pyserial")

ap = argparse.ArgumentParser()
ap.add_argument("port"); ap.add_argument("out")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--seconds", type=float, default=0, help="0 = ate Ctrl+C")
a = ap.parse_args()
end = time.time() + a.seconds if a.seconds else None
with serial.Serial(a.port, a.baud, timeout=0.5) as s, open(a.out, "w", encoding="utf-8") as f:
    print(f"Gravando {a.port} -> {a.out} (Ctrl+C para parar). Aperte RESET na placa para registrar desde o boot.")
    try:
        while end is None or time.time() < end:
            ln = s.readline().decode("utf-8", "replace").rstrip("\r\n")
            if ln:
                print(ln); f.write(ln + "\n"); f.flush()
    except KeyboardInterrupt:
        pass
