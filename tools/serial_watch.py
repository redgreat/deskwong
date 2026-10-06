import serial, time, sys

port = serial.Serial("COM7", 115200, timeout=1)
deadline = time.time() + int(sys.argv[1]) if len(sys.argv) > 1 else time.time() + 45
buf = b""
while time.time() < deadline:
    chunk = port.read(4096)
    if chunk:
        buf += chunk
        sys.stdout.write(chunk.decode("utf-8", errors="replace"))
        sys.stdout.flush()
port.close()
