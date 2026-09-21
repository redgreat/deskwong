@echo off
REM 生成 web 静态资源的 SPIFFS 镜像（Windows，需 ESP-IDF 命令提示符环境）
REM 用法：在 ESP-IDF 终端执行 tools\make_spiffs.bat
setlocal
set ROOT=%~dp0..
set WEB_DIST=%ROOT%\web\dist
set SPIFFS_SIZE=0x7E0000
set OUT=%ROOT%\firmware\build\spiffs.bin
set TMP=%ROOT%\build\spiffs_tmp

echo ==^> 构建前端 web/dist ...
pushd "%ROOT%\web"
call npm install
if errorlevel 1 (popd & exit /b 1)
call npm run build
if errorlevel 1 (popd & exit /b 1)
popd

echo ==^> 打包 SPIFFS（www/） ...
if exist "%TMP%" rmdir /s /q "%TMP%"
mkdir "%TMP%\www"
xcopy /s /e /y "%WEB_DIST%\*" "%TMP%\www\" >nul
if errorlevel 1 exit /b 1
if not exist "%ROOT%\firmware\build" mkdir "%ROOT%\firmware\build"
python "%IDF_PATH%\components\spiffs\spiffsgen.py" %SPIFFS_SIZE% "%TMP%" "%OUT%"
if errorlevel 1 exit /b 1
rmdir /s /q "%TMP%"
echo ==^> 生成 %OUT%
