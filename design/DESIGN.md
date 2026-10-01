# 设备界面设计基线

`firmware/components/app_ui/main_screen.cpp` 与 `sync_screen.cpp` 是 400×300 墨水屏界面的唯一布局事实来源；本目录不维护与代码分离的手绘稿。

## 最新界面

- `final/main-screen.png`：正常月与联网数据。
- `final/main-screen-six-row.png`：六行月份边界。
- `final/main-screen-empty.png`：离线、未配置和空数据状态。
- `final/racebox-sync.png`：RaceBox 下载完成后的上传进度、速度和设备信息。

## 生成与验收

运行 `cmake -S test/ui_preview -B build/ui_preview -G Ninja -DCMAKE_BUILD_TYPE=Release`，编译 `ui_preview`，按 `test/ui_preview/README.md` 生成四个 PGM，再等比转换为 PNG。预览直接链接生产 UI、日历服务与字库，因此用于发现裁切、缺字、六行月溢出、进度条和状态文案问题。

主机预览不能证明实机的 SPI 时序、PSRAM、刷新方向或灰阶阈值正确；发布前仍须完成固件构建、刷写哈希校验和物理屏幕核对。
