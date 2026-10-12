"""Dump on-device partition table, otadata and app images for diagnosis."""
import struct
import subprocess
import sys

PY = sys.executable
PORT = sys.argv[1] if len(sys.argv) > 1 else "COM7"
OUT = "build/dev_diag.bin"


def read_flash(addr, size, path):
    r = subprocess.run(
        [PY, "-m", "esptool", "--port", PORT, "--no-stub", "read_flash",
         hex(addr), hex(size), path],
        capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print("read 0x%x failed: %s" % (addr, (r.stderr or r.stdout)[-400:]))
        return None
    with open(path, "rb") as f:
        return f.read()


# 1. partition table @ 0x8000
p = read_flash(0x8000, 0xC00, OUT + ".ptable")
print("=== partition table @0x8000 ===")
apps = []
if p:
    for i in range(0, 0xC00, 32):
        e = p[i:i + 32]
        if len(e) < 32 or e[0] != 0xAA or e[1] != 0x50:
            if e[0] == 0xEB and e[1] == 0xEB:
                print("  [md5 marker]")
            continue
        typ, sub = e[2], e[3]
        off, size = struct.unpack_from("<II", e, 4)
        lbl = e[12:28].split(b"\x00")[0].decode("ascii", "replace")
        print("  %-10s type=%d sub=%d off=0x%06x size=0x%06x" % (lbl, typ, sub, off, size))
        if typ == 0:  # app
            apps.append((lbl, off, size))

# 2. otadata: try both common locations
for oa in (0xE000, 0xF000):
    d = read_flash(oa, 0x2000, OUT + ".otadata")
    if not d:
        continue
    print("=== otadata @0x%x ===" % oa)
    for base in (0, 0x1000):
        seq = struct.unpack_from("<I", d, base)[0]
        crc = struct.unpack_from("<I", d, base + 24)[0]
        print("  slot[%d]: ota_seq=%d(0x%x) crc=0x%08x" % (base // 0x1000, seq, seq, crc))

# 3. inspect each app partition + common offsets
offsets = sorted({off for _, off, _ in apps} | {0x10000, 0x20000, 0x420000})
for off in offsets:
    d = read_flash(off, 0x100, OUT + ".img")
    if not d:
        continue
    if d[0] != 0xE9:
        print("=== 0x%06x: NOT an app image (first bytes %r) ===" % (off, d[:16]))
        continue
    # esp_app_desc at +0x20
    desc = read_flash(off + 0x20, 0x100, OUT + ".desc")
    if not desc or struct.unpack_from("<I", desc, 0)[0] != 0xABCD5432:
        print("=== 0x%06x: image header ok, app_desc invalid ===" % off)
        continue
    f = lambda b: b.split(b"\x00")[0].decode("utf-8", "replace")
    print("=== app @0x%06x ===" % off)
    print("  project : %s" % f(desc[0x30:0x50]))
    print("  version : %s" % f(desc[0x10:0x30]))
    print("  time    : %s  date: %s" % (f(desc[0x50:0x60]), f(desc[0x60:0x70])))
    print("  idf     : %s" % f(desc[0x70:0x90]))
    print("  elf_sha : %s" % desc[0x90:0xB0].hex())
