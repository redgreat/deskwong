#!/usr/bin/env python3
"""量测主屏顶部状态栏左簇（电量 / 网络 / 语音）的真实墨迹排版。

读取 UI 预览渲染出的 400x300 PGM（P5，0=黑 255=白），按「空列」把左簇切成
一个个元素，输出每个元素的墨迹横向范围、纵向范围和纵向中心，以及相邻元素
之间的水平间隙。用于验证 REQUIREMENTS §2：状态栏所有图标与文字按同一条水平
视觉中心线上下居中，且电量位数变化（1% ~ 100%）时不互相挤压。

用法：
    python test/ui_preview/status_bar_check.py build/ui_preview/states/full.pgm
    python test/ui_preview/status_bar_check.py <pgm> --assert
"""
import sys

W, H = 400, 300
# 状态栏有效区 y=0..31，视觉中心线 y=16；x 上到第一根分隔线（x=116）之前为止。
BAR_TOP, BAR_BOTTOM = 0, 31
BAR_RIGHT = 114
CENTER_Y = 16
MIN_GAP = 4          # 相邻元素之间允许的最小水平间隙（像素）
CENTER_TOL = 2       # 墨迹中心相对 y=16 的允许偏差（像素）


def load_pgm(path):
    with open(path, "rb") as f:
        data = f.read()
    # P5\n400 300\n255\n<像素>
    parts = data.split(b"\n", 3)
    if parts[0].strip() != b"P5":
        raise SystemExit("只支持 P5 PGM: %s" % path)
    w, h = (int(v) for v in parts[1].split())
    if (w, h) != (W, H):
        raise SystemExit("尺寸应为 400x300，实际 %dx%d" % (w, h))
    return parts[3]


def analyze(px):
    """返回左簇元素列表：{'x1','x2','y1','y2','cy'}"""
    ink_cols = []
    for x in range(0, BAR_RIGHT + 1):
        hit = False
        for y in range(BAR_TOP, BAR_BOTTOM + 1):
            if px[y * W + x] < 128:
                hit = True
                break
        ink_cols.append(hit)

    runs = []
    x = 0
    while x <= BAR_RIGHT:
        if not ink_cols[x]:
            x += 1
            continue
        start = x
        while x <= BAR_RIGHT and ink_cols[x]:
            x += 1
        runs.append((start, x - 1))

    # 同一元素内部（数字的每一位、汉字的笔画间隙）只隔 1~2 列，属于一个元素；
    # 只有 >=3 列空白才算元素之间的间隔。
    merged = []
    for run in runs:
        if merged and run[0] - merged[-1][1] - 1 < 3:
            merged[-1] = (merged[-1][0], run[1])
        else:
            merged.append(run)

    items = []
    for start, end in merged:
        ys = [y for y in range(BAR_TOP, BAR_BOTTOM + 1)
              if any(px[y * W + c] < 128 for c in range(start, end + 1))]
        if ys:
            items.append({"x1": start, "x2": end, "y1": min(ys), "y2": max(ys),
                          "cy": (min(ys) + max(ys)) / 2.0})
    return items


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    do_assert = "--assert" in sys.argv
    if not args:
        raise SystemExit("用法: status_bar_check.py <pgm> [--assert]")
    px = load_pgm(args[0])
    items = analyze(px)

    print("%s" % args[0])
    if not items:
        print("  状态栏左簇没有墨迹")
        if do_assert:
            raise SystemExit("状态栏左簇为空")
        return 0
    prev = None
    problems = []
    for it in items:
        gap = "" if prev is None else "%+d" % (it["x1"] - prev["x2"] - 1)
        print("  x %3d..%-3d  y %2d..%-2d  中心 y=%5.1f  间隙 %s"
              % (it["x1"], it["x2"], it["y1"], it["y2"], it["cy"], gap))
        if prev is not None:
            g = it["x1"] - prev["x2"] - 1
            if g < MIN_GAP:
                problems.append("x=%d..%d 与 x=%d..%d 间隙仅 %dpx（需 >=%d）"
                                % (prev["x1"], prev["x2"], it["x1"], it["x2"], g, MIN_GAP))
        if abs(it["cy"] - CENTER_Y) > CENTER_TOL:
            problems.append("x=%d..%d 墨迹中心 y=%.1f 偏离中心线 %d 超过 %dpx"
                            % (it["x1"], it["x2"], it["cy"], CENTER_Y, CENTER_TOL))
        prev = it

    if problems:
        print("  不合格：")
        for p in problems:
            print("    - %s" % p)
        if do_assert:
            raise SystemExit("状态栏排版检查未通过")
        return 1
    print("  合格：最小间隙 >= %dpx，所有元素墨迹中心在 y=%d±%d 内"
          % (MIN_GAP, CENTER_Y, CENTER_TOL))
    return 0


if __name__ == "__main__":
    sys.exit(main())
