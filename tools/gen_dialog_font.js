// 生成对话字体（GB2312 一级汉字 @14px）。
// 为什么不直接在 generate_ui_fonts.ps1 里传 --symbols：PowerShell 5.1 向原生命令
// 传 >3KB 非 ASCII 参数会按系统代码页（GBK）重编码，损坏字符甚至让 node 崩溃
// （libuv async 断言）。node 的 execFileSync 走 CreateProcess UTF-16，无此问题。
//
// 用法：node gen_dialog_font.js <lv_font_conv.js 路径>
'use strict';
const { execFileSync } = require('child_process');
const path = require('path');

const conv = process.argv[2];
if (!conv) {
  console.error('usage: node gen_dialog_font.js <lv_font_conv.js>');
  process.exit(2);
}
const root = path.join(__dirname, '..');

const bytes = [];
for (let hi = 0xa1; hi <= 0xa9; hi++) {         // GB2312 符号区：全角标点、、希腊等
  for (let lo = 0xa1; lo <= 0xfe; lo++) {
    bytes.push(hi, lo);
  }
}
for (let hi = 0xb0; hi <= 0xd7; hi++) {         // GB2312 一级字库区 0xB0A1..0xD7F9
  for (let lo = 0xa1; lo <= 0xfe; lo++) {
    bytes.push(hi, lo);                          // 3755 常用汉字（个别空码由字体忽略）
  }
}
// GB2312 是双字节编码，必须经 GBK 解码成 Unicode 码点；直接按码位 fromCharCode
// 会落进韩文区，simhei 一个都匹配不上。
const sym = new TextDecoder('gbk').decode(Uint8Array.from(bytes));

const out = path.join(root, 'firmware/components/app_ui/font_dialog_14.c');
execFileSync('node', [
  conv,
  '--font', 'C:/Windows/Fonts/simhei.ttf',
  '-r', '0x20-0x7E',
  '--symbols', sym,
  '--size', '14',
  '--bpp', '1',
  '--no-compress',
  '--format', 'lvgl',
  '--lv-font-name', 'lv_font_dialog_14',
  '--no-kerning',
  '-o', out,
], { stdio: 'inherit' });
console.log('dialog font written:', out);
