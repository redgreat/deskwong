"""压紧生成字体里 ° (U+00B0) 的字模：左边距归零、步进收窄到圈宽减 1px。

lv_font_conv 按 simhei 原始度量输出，° 占半个字宽还带左边距，跟在数字后面
显得松散。字模位图注释（/* U+00B0 "°" */）与 glyph_dsc 条目按顺序一一对应
（dsc[0] 为保留项），据此定位并改写 adv_w / ofs_x。由 tools/generate_ui_fonts.ps1
在字体生成后自动调用。
"""
import re, io

SIZES = (10, 14, 20)


def patch(path: str) -> None:
    src = io.open(path, encoding='utf-8').read()
    bm = re.search(r'glyph_bitmap\[\] = \{(.*?)\n\};', src, re.S)
    marks = [(int(m.group(1), 16), m.start())
             for m in re.finditer(r'/\* U\+([0-9A-Fa-f]+) "[^"]*" \*/', src)
             if bm.span(1)[0] < m.start() < bm.span(1)[1]]
    dsc_m = re.search(r'glyph_dsc\[\] = \{(.*?)\n\};', src, re.S)
    dsc_text = dsc_m.group(1)

    for idx, (uni, _) in enumerate(marks):
        if uni != 0xB0:
            continue
        entries = list(re.finditer(
            r'\{\.bitmap_index = (-?\d+), \.adv_w = (\d+), \.box_w = (\d+),'
            r' \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}', dsc_text))
        e = entries[idx + 1]  # id 0 为保留项
        box_w = int(e.group(3))
        new_adv = max(2, box_w - 1) * 16  # 单位 1/16px；允许后继 / 靠近字圈
        replaced = (f'{{.bitmap_index = {e.group(1)}, .adv_w = {new_adv}, .box_w = {e.group(3)},'
                    f' .box_h = {e.group(4)}, .ofs_x = 0, .ofs_y = {e.group(6)}}}')
        dsc_text = dsc_text[:e.start()] + replaced + dsc_text[e.end():]
        print(f'{path}: ° adv -> {new_adv/16:.0f}px, ofs_x -> 0')
        break
    else:
        raise SystemExit(f'{path}: degree glyph not found')

    src = src[:dsc_m.start(1)] + dsc_text + src[dsc_m.end(1):]
    io.open(path, 'w', encoding='utf-8', newline='').write(src)


for s in SIZES:
    patch(f'firmware/components/app_ui/font_zh_{s}.c')
