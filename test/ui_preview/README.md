# 主屏真实渲染预览

直接编译固件 `main_screen.cpp`、字库和农历服务，使用项目的 LVGL 8.4 渲染器。
flush 采用与固件相同的 RGB565 → 黑白阈值，不重新实现一份网页/绘图布局。
工时、天气、额度和 RaceBox 数量为演示数据，农历由真实服务计算。

```powershell
cmake -S test/ui_preview -B build/ui_preview -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui_preview --target ui_preview -j 8
build/ui_preview/ui_preview.exe build/main-screen.pgm
build/ui_preview/ui_preview.exe build/main-screen-six-row.pgm 3
build/ui_preview/ui_preview.exe build/main-screen-empty.pgm 9 empty
build/ui_preview/ui_preview.exe build/racebox-sync.pgm 9 sync
build/ui_preview/ui_preview.exe build/main-screen-battery-full.pgm 9 full
build/ui_preview/ui_preview.exe build/main-screen-battery-low.pgm 9 low
build/ui_preview/ui_preview.exe build/main-screen-voice-off.pgm 9 voice-off
```

`full` / `low` 渲染电量 100% 与 9%（顶栏电量文本最宽和最窄两种状态），
`voice-off` 渲染后台「启用语音」关闭的状态，用于核对小智图标隐藏且不推动后续元素。

需要 C/C++ 编译器、CMake、Ninja。输出为 400×300 PGM；可用 Pillow 转 PNG。
断言检查中文字符在禁用字体压缩支持时仍有字形，并检查 LVGL 堆剩余空间。
固件与预览堆均为 128 KB；当前完整界面约占 87 KB。

## 顶栏排版量测

```powershell
python test/ui_preview/status_bar_check.py build/ui_preview/states/battery-full.pgm --assert
python test/ui_preview/pgm_to_png.py build/ui_preview/states/battery-full.pgm build/topbar.png --crop 0,0,130,31 --zoom 5
```

`status_bar_check.py` 按空列把主屏顶部状态栏左簇切成元素，输出每个元素的墨迹横向范围、
纵向范围和中心，并检查相邻元素最小水平间隙 ≥4px、各元素墨迹中心在 y=16±2 内
（SPECIFICATIONS §5）。`pgm_to_png.py` 用标准库把 PGM 转 PNG，支持裁剪和整数倍放大，
用来人眼核对 30px 高的顶栏这类小区域。

`tools/generate_ui_fonts.ps1` 生成未压缩 1bpp 字库；`-Converter` 可指定已安装的
`lv_font_conv.js` 路径，使用 Windows 本地 SimHei 和 Arial Bold 字体。

预览覆盖排版和像素转换，不能替代屏幕 SPI 传输、反射对比度、实际服务数据的硬件验收。
