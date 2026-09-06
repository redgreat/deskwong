@echo off
REM 一键烧录固件 + web 资源（Windows，需 ESP-IDF 命令提示符环境，串口 COM 需按需修改）
REM 用法：在 ESP-IDF 终端执行 tools\flash.bat
setlocal
set ROOT=%~dp0..
set PORT=COM3
set BAUDRATE=460800

echo ==^> 编译固件 ...
pushd %ROOT%\firmware
call idf.py build
popd

echo ==^> 生成 SPIFFS ...
call %ROOT%\tools\make_spiffs.bat

echo ==^> 烧录到 %PORT% ...
python -m esptool --chip esp32s3 -b %BAUDRATE% -p %PORT% write_flash ^
  0x0 %ROOT%\firmware\build\bootloader\bootloader.bin ^
  0x8000 %ROOT%\firmware\build\partition_table\partition-table.bin ^
  0x20000 %ROOT%\firmware\build\deskwong.bin ^
  0x820000 %ROOT%\firmware\build\spiffs.bin

echo ==^> 完成
