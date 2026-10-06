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

# lunar-go 宜忌活动名称会随日期变化，不能只从当前 C/C++ 字符串反推字库。
# 这里固定审计服务端可能返回的完整活动词表字符，避免某天才暴露方块字。
ALMANAC_GLYPHS = set(
    "丧乘买事井产亲人伐作佣做光养冠券刻剃割勿匾卸厕合嗣坏坟垣基堤塑塞墙头"
    "婚婿宁容寿居屋岫帐平库庙归徙扇拆挂挽捉捕掘探放教斋普木机架染柩柱桥梁"
    "械殓池治涂渔渠渡火灶灸焚牛牧猎甲畋畜疗病盖盟眼碑碓磉磑神移稠穴穿竖笄"
    "筑绘脊船艺蚁蜜衣补裁见订讼词诸谢财货贵起车道酝酬酿醮针钻铸门问陂雇雕"
    "面饰馀香马鼓齐祭祀沐浴修平取嫁娶出行上入宅"
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
                chars = set(re.findall(r"[\u2103\u3400-\u9fff\u3000-\u303f\uff00-\uffef]", "".join(literals)))
                used.update(chars)
                references[path] = chars

    generator = (ROOT / "tools/generate_ui_fonts.ps1").read_text(encoding="utf-8")
    match = re.search(r"\$symbols\s*=\s*'([^']*)'", generator)
    if not match:
        print("Cannot find $symbols in tools/generate_ui_fonts.ps1", file=sys.stderr)
        return 2
    available = set(match.group(1))
    required = used | ALMANAC_GLYPHS
    missing = required - available
    if missing:
        print("Missing UI glyphs:", "".join(sorted(missing)), file=sys.stderr)
        for path, chars in references.items():
            absent = chars - available
            if absent:
                print(f"  {path.relative_to(ROOT)}: {''.join(sorted(absent))}", file=sys.stderr)
        return 1
    print(
        f"UI font audit passed: {len(used)} source characters and "
        f"{len(ALMANAC_GLYPHS)} almanac characters covered"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
