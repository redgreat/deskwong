param([string]$Converter = "$env:LOCALAPPDATA/npm-cache/_npx/b62fd1a864044392/node_modules/lv_font_conv/lv_font_conv.js")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$symbols = '但理计秒功口号名域密效析查检法码络网误账错化始配前当段阶史过进本次结果汇总关闭确认保留复长内足校验异常及长度多日一二三四五六七八九十正冬腊闰初廿小寒大立春雨水惊蛰清明谷夏满芒种暑秋处白露霜降雪至元旦节劳动端午中国庆调休上班晴多云阴雾霾风月年时分周期农历黄天今室内北京杭州阵雷暴夹浮尘扬沙热冷未知准备企微签到签退记得记录工宜忌专注拖延每同步已连接失败扫描下载上传清除天气五周额度暂无数据不可用剩余温湿度设备搜索蓝牙正在读取状态轨迹服务器再次短按返回主屏完成空闲发现条取消令位例冰冻命如存少强待断毛等米纬经置要试请超送重雹需响应拒绝解锁间伴有仍极端降细特到薄浓度严重龙卷缓边流式停止任启批未来后台接口出行会友签约学习整理沐浴扫舍动土搬家远开仓争执祭祀祈福嫁娶纳采开市安床修造入宅交易求医栽种破土安葬赴任于住先克具写冲创制却厂只地址字定实客就帧建志忽恢户手指提握播文新方更束松标栈样模没烧片率生略的绪编话询语跑轮部键非音频麦°℃，。！？：；（）、聆你区句否听唤型好引擎是智柄监稍稳获醒里量受丧乘买事井产亲人伐作佣做光养冠券刻剃割勿匾卸厕合嗣坏坟垣基堤塑塞墙头婚婿宁容寿居屋岫帐平库庙归徙扇拆挂挽捉捕掘探放教斋普木机架染柩柱桥梁械殓池治涂渔渠渡火灶灸焚牛牧猎甲畋畜疗病盖盟眼碑碓磉磑神移稠穴穿竖笄筑绘脊船艺蚁蜜衣补裁见订讼词诸谢财货贵起车道酝酬酿醮针钻铸门问陂雇雕面饰馀香马鼓齐'
foreach ($size in @(10,14,20)) {
    & node $Converter --font C:/Windows/Fonts/simhei.ttf -r 0x20-0x7E --symbols $symbols --size $size --bpp 1 --no-compress --format lvgl --lv-font-name "lv_font_zh_$size" --no-kerning -o "$root/firmware/components/app_ui/font_zh_$size.c"
    if ($LASTEXITCODE) { throw 'Font generation failed' }
}
# simhei 的 ° 步进偏宽（占半个字宽还带左边距），生成后压紧贴住数字
& python "$root/tools/patch_font_degree.py"
if ($LASTEXITCODE) { throw 'degree patch failed' }

# 对话字体：GB2312 一级汉字（3755 字）@14px，覆盖 AI 回复的常用字。
# 符号串太大且非 ASCII，必须经 node 包装脚本生成（见 gen_dialog_font.js 头注释），
# 不能在 PowerShell 里直接传 --symbols。
& node "$root/tools/gen_dialog_font.js" $Converter
if ($LASTEXITCODE) { throw 'dialog font generation failed' }
foreach ($size in @(18,28,36)) {
    & node $Converter --font C:/Windows/Fonts/arialbd.ttf -r 0x20-0x7E --size $size --bpp 1 --no-compress --format lvgl --lv-font-name "lv_font_digits_$size" --no-kerning -o "$root/firmware/components/app_ui/font_digits_$size.c"
    if ($LASTEXITCODE) { throw 'Font generation failed' }
}
