import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'

const source = await readFile(new URL('../../web/src/pages/Login.svelte', import.meta.url), 'utf8')

assert.match(source, /let\s+user\s*=\s*['"]['"]/, '登录账号初始值应为空')
assert.doesNotMatch(source, /let\s+user\s*=\s*['"]admin['"]/, '登录页不得硬编码默认 admin 账号')
assert.match(source, /autocomplete=["']username["']/, '应保留浏览器用户名自动填充语义')
assert.match(source, /autocomplete=["']current-password["']/, '应保留浏览器密码自动填充语义')

console.log('Web login defaults inspection passed')
