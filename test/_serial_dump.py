import sys
import time
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM7"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

s = serial.Serial(port, baud, timeout=0.2)
# Do not assert DTR/RTS: keep the chip running, we only want to listen.
s.dtr = False
s.rts = False
end = time.time() + secs
buf = b""
while time.time() < end:
    buf += s.read(4096)
s.close()
sys.stdout.write(buf.decode("utf-8", "replace"))
