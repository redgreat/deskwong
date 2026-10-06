"""生成 deskwong 后台 favicon：深色圆角底 + 白色迷你日历（与设备顶栏图标同款）。"""
import PIL.Image, PIL.ImageDraw, os

def draw_calendar(size):
    img = PIL.Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = PIL.ImageDraw.Draw(img)
    s = size / 32.0
    # 深色圆角底
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=int(7 * s), fill=(15, 20, 34, 255))
    w = max(1, round(2 * s))
    m = round(5 * s)
    # 白色日历边框 + 实心标题条
    d.rounded_rectangle([m, m, size - 1 - m, size - 1 - m], radius=int(2.5 * s), outline=(235, 240, 250, 255), width=w)
    hm = m + w + round(1 * s)
    d.rounded_rectangle([hm, hm, size - 1 - hm, hm + round(3.5 * s)], radius=int(1 * s), fill=(235, 240, 250, 255))
    # 2x2 日期点
    dot = max(1, round(2.6 * s))
    body_top = hm + round(3.5 * s) + round(2 * s)
    body_bot = size - 1 - m - w - round(1 * s)
    xs = [m + round(4 * s), size - 1 - m - round(4 * s) - dot]
    mid = (body_top + body_bot - dot) / 2
    ys = [round(mid), round(body_bot - dot)]
    for x in xs:
        for y in ys:
            d.rounded_rectangle([x, y, x + dot, y + dot], radius=max(1, dot // 3), fill=(235, 240, 250, 255))
    return img

os.makedirs("web/public", exist_ok=True)
base = draw_calendar(64)
base.save("web/public/favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])
base.resize((32, 32), PIL.Image.LANCZOS).save("build/favicon_preview.png")
print("favicon.ico written")
