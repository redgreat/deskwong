"""Rasterize QWeather's MIT SVG weather icons for the 1-bit LVGL display.

Run with a local checkout of https://github.com/qwd/Icons as the argument.
Requires cairosvg and Pillow. Output is checked in so firmware builds need neither.
"""
from pathlib import Path
import re
import sys

import cairosvg
from PIL import Image
from io import BytesIO


source = Path(sys.argv[1]) / "icons"
target = Path(__file__).resolve().parents[1] / "firmware/components/app_ui/weather_icons.inc"
codes = sorted(int(p.stem) for p in source.glob("*.svg")
               if re.fullmatch(r"\d{3}", p.stem))
lines = ["// QWeather Icons, Copyright (c) 2021 QWeather, MIT license.",
         "// Source: https://github.com/qwd/Icons; 32x32, 1-bit alpha."]
for code in codes:
    png = cairosvg.svg2png(url=str(source / f"{code}.svg"), output_width=32, output_height=32)
    image = Image.open(BytesIO(png)).convert("RGBA")
    bits = [int(image.getpixel((x, y))[3] >= 96) for y in range(32) for x in range(32)]
    packed = [sum(bits[i + b] << (7 - b) for b in range(8)) for i in range(0, len(bits), 8)]
    lines.append(f"static const uint8_t weather_{code}[] = {{")
    lines.extend("    " + ", ".join(f"0x{v:02x}" for v in packed[i:i+16]) + ","
                 for i in range(0, len(packed), 16))
    lines.append("};")
lines.append("static const struct { int code; const uint8_t *data; } weather_icons[] = {")
lines.extend(f"    {{{code}, weather_{code}}}," for code in codes)
lines.append("};")
target.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Wrote {len(codes)} icons to {target}")
