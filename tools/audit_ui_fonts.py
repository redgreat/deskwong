"""Fail when a Chinese character used by firmware UI/messages is absent from UI fonts."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE_DIRS = (
    ROOT / "firmware/components/app_ui",
    ROOT / "firmware/components/app_services",
    ROOT / "firmware/main",
)


def main() -> int:
    used: set[str] = set()
    references: dict[pathlib.Path, set[str]] = {}
    for directory in SOURCE_DIRS:
        for pattern in ("*.c", "*.cpp", "*.h"):
            for path in directory.glob(pattern):
                if path.name.startswith("font_"):
                    continue
                source = path.read_text(encoding="utf-8", errors="ignore")
                literals = re.findall(r'"(?:\\.|[^"\\])*"', source)
                chars = set(re.findall(r"[\u3400-\u9fff\u3000-\u303f\uff00-\uffef]", "".join(literals)))
                used.update(chars)
                references[path] = chars

    generator = (ROOT / "tools/generate_ui_fonts.ps1").read_text(encoding="utf-8")
    match = re.search(r"\$symbols\s*=\s*'([^']*)'", generator)
    if not match:
        print("Cannot find $symbols in tools/generate_ui_fonts.ps1", file=sys.stderr)
        return 2
    available = set(match.group(1))
    missing = used - available
    if missing:
        print("Missing UI glyphs:", "".join(sorted(missing)), file=sys.stderr)
        for path, chars in references.items():
            absent = chars - available
            if absent:
                print(f"  {path.relative_to(ROOT)}: {''.join(sorted(absent))}", file=sys.stderr)
        return 1
    print(f"UI font audit passed: {len(used)} Chinese characters/punctuation covered")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
