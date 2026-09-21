# 主屏真实渲染预览

直接编译固件 `main_screen.cpp`、字库和农历服务，使用项目的 LVGL 8.4 渲染器。
flush 采用与固件相同的 RGB565 → 黑白阈值，不重新实现一份网页/绘图布局。
工时、天气、额度和 RaceBox 数量为演示数据，农历由真实服务计算。

```powershell
cmake -S tools/ui_preview -B build/ui_preview -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui_preview --target ui_preview -j 8
build/ui_preview/ui_preview.exe build/ui-preview.pgm
build/ui_preview/ui_preview.exe build/ui-preview-six.pgm 3
build/ui_preview/ui_preview.exe build/ui-preview-empty.pgm 9 empty
```

需要 C/C++ 编译器、CMake、Ninja。输出为 400×300 PGM；可用 Pillow 转 PNG。
断言检查中文字符在禁用字体压缩支持时仍有字形，并检查 LVGL 堆剩余空间。
固件与预览堆均为 128 KB；当前完整界面约占 87 KB。

`tools/generate_ui_fonts.ps1` 生成未压缩 1bpp 字库；`-Converter` 可指定已安装的
`lv_font_conv.js` 路径，使用 Windows 本地 SimHei 和 Arial Bold 字体。

预览覆盖排版和像素转换，不能替代屏幕 SPI 传输、反射对比度、实际服务数据的硬件验收。
