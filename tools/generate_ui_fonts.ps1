param([string]$Converter = "$env:LOCALAPPDATA/npm-cache/_npx/b62fd1a864044392/node_modules/lv_font_conv/lv_font_conv.js")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$symbols = '日一二三四五六七八九十正冬腊闰初廿小寒大立春雨水惊蛰清明谷夏满芒种暑秋处白露霜降雪至元旦节劳动端午中国庆调休上班晴多云阴雾霾风月年时分周期农历天今室内北京杭州阵雷暴夹浮尘扬沙热冷未知准备企微签到签退记得记录工宜忌专注拖延每同步已连接失败扫描下载上传清除天气五周额度暂无数据不可用剩余温湿度设备搜索蓝牙正在读取状态轨迹服务器再次短按返回主屏完成空闲发现条取消令位例冰冻命如存少强待断毛等米纬经置要试请超送重雹需响应拒绝解锁间伴有极端降细特到薄浓度严重龙卷，。！？：；（）'
foreach ($size in @(10,14,20)) {
    & node $Converter --font C:/Windows/Fonts/simhei.ttf -r 0x20-0x7E --symbols $symbols --size $size --bpp 1 --no-compress --format lvgl --lv-font-name "lv_font_zh_$size" --no-kerning -o "$root/firmware/components/app_ui/font_zh_$size.c"
    if ($LASTEXITCODE) { throw 'Font generation failed' }
}
foreach ($size in @(18,28,36)) {
    & node $Converter --font C:/Windows/Fonts/arialbd.ttf -r 0x20-0x7E --size $size --bpp 1 --no-compress --format lvgl --lv-font-name "lv_font_digits_$size" --no-kerning -o "$root/firmware/components/app_ui/font_digits_$size.c"
    if ($LASTEXITCODE) { throw 'Font generation failed' }
}
