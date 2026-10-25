import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'

const page = await readFile(new URL('../../web/src/pages/Dashboard.svelte', import.meta.url), 'utf8')
const api = await readFile(new URL('../../web/src/lib/api.js', import.meta.url), 'utf8')

/* 设备后台必须提供 /api/ai 读取实时用量 */
assert.match(api, /getAI:\s*\(\)\s*=>\s*req\('\/api\/ai'\)/, 'api.js 应提供 getAI 读取 /api/ai')

/* 四选二：绑定到设备配置字段，最多两个、至少保留一个；选满后点击要替换而不是无响应 */
assert.match(page, /aiusage_apps:\s*'chatgpt'/, 'AI 展示应用应有 chatgpt 默认值')
assert.match(page, /if\s*\(list\.length\s*>=\s*2\)\s*list\.shift\(\)/, '选满两个后再点应替换最早选的应用，不能无响应')
assert.match(page, /if\s*\(list\.length\s*<=\s*1\)\s*return/, '至少保留一个展示应用')
assert.match(page, /<button type="button" class="app-chip" class:on aria-pressed=\{on\}/, '应用选择应是可点击的按钮（tab）')
assert.doesNotMatch(page, /disabled=\{\!on\s*&&/, '不得禁用未选中的应用按钮')

/* 下方按目录展示全部应用的实时进度条（有数据画进度条，没数据显示占位） */
assert.match(page, /\$:\s*usageCards\s*=\s*catalog/, '应从应用目录生成用量卡片')
assert.match(page, /usage-fill[^>]*style="width:\s*\{w\.remaining_percent\s*==\s*null\s*\?\s*0\s*:\s*w\.remaining_percent\}%"/,
  '进度条宽度应直接取剩余百分比')
assert.match(page, /w\.remaining_percent\s*==\s*null\s*\?\s*'--%'/, '未知剩余百分比应显示 --%')
assert.match(page, /暂无用量数据/, '无数据时应显示占位文案')

/* 界面文案：统一叫 ChatGPT，且不保留说明性小字 */
assert.match(page, /name:\s*'ChatGPT'/, '应用目录应显示 ChatGPT')
assert.doesNotMatch(page, /name:\s*'GPT'/, '不得再显示 GPT 短名')
assert.doesNotMatch(page, /<small>/, '不得保留说明性小字')

/* 会定时刷新实时数据 */
assert.match(page, /setInterval\(refreshAI,/, '应定时刷新 AI 用量')
assert.doesNotMatch(page, /aiusage_apps:\s*''/, 'AI 展示应用不得留空（留空会被固件退回 chatgpt）')

console.log('Web AI usage 4-choose-2 inspection passed')
