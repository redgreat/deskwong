"""Read local serial boot diagnostics without printing WiFi names or credentials."""
import argparse
import re
import time
from pathlib import Path

import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM7')
parser.add_argument('--seconds', type=float, default=15)
parser.add_argument('--output', default='build/ui2-device-boot.log')
parser.add_argument('--reset', action='store_true', help='Restart connected USB-JTAG board before capture')
args = parser.parse_args()
port = serial.Serial()
port.port, port.baudrate, port.timeout = args.port, 115200, 0.2
port.dtr = port.rts = False
port.open()
if args.reset:
    from esptool.reset import HardReset
    HardReset(port, uses_usb=True)()
data = bytearray()
try:
    until = time.monotonic() + args.seconds
    while time.monotonic() < until:
        data.extend(port.read(8192))
finally:
    port.close()
Path(args.output).write_bytes(data)
pattern = r'UI2|boot done|first display|UI pool|LvglPort|assert|panic|Guru|abort|allocate|App version:|Compile time:|Loaded app|got ip|SoftAP|http server'
for line in data.decode('utf-8', 'replace').splitlines():
    if re.search(pattern, line):
        print(line.encode('ascii', 'backslashreplace').decode())
print(f'Captured {len(data)} bytes into {args.output}')
