#!/usr/bin/env python3
"""把 UI 预览渲染的 400x300 PGM 转成 PNG，便于人眼核对排版。

纯标准库实现（zlib + struct），不依赖 Pillow。可选裁剪与整数倍放大，
用来放大检查顶部状态栏这类只有 30 像素高的区域。

用法：
    python test/ui_preview/pgm_to_png.py <in.pgm> <out.png> [--crop x1,y1,x2,y2] [--zoom N]
"""
import struct
import sys
import zlib


def load_pgm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 3)
    if parts[0].strip() != b"P5":
        raise SystemExit("只支持 P5 PGM")
    w, h = (int(v) for v in parts[1].split())
    return w, h, parts[3]


def write_png(path, w, h, rows):
    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        return c + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    args = []
    opts = {}
    i = 1
    while i < len(sys.argv):
        a = sys.argv[i]
        if a == "--crop":
            opts["crop"] = [int(v) for v in sys.argv[i + 1].split(",")]
            i += 2
            continue
        if a == "--zoom":
            opts["zoom"] = int(sys.argv[i + 1])
            i += 2
            continue
        args.append(a)
        i += 1
    if len(args) < 2:
        raise SystemExit("用法: pgm_to_png.py <in.pgm> <out.png> [--crop x1,y1,x2,y2] [--zoom N]")

    w, h, px = load_pgm(args[0])
    x1, y1, x2, y2 = opts.get("crop", [0, 0, w - 1, h - 1])
    zoom = opts.get("zoom", 1)
    rows = []
    for y in range(y1, y2 + 1):
        row = []
        for x in range(x1, x2 + 1):
            v = px[y * w + x]
            row.extend([v] * zoom)
        for _ in range(zoom):
            rows.append(row)
    write_png(args[1], (x2 - x1 + 1) * zoom, (y2 - y1 + 1) * zoom, rows)
    print("%s -> %s (%dx%d, zoom=%d)" % (args[0], args[1],
                                         (x2 - x1 + 1) * zoom, (y2 - y1 + 1) * zoom, zoom))


if __name__ == "__main__":
    main()
