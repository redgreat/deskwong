import assert from 'node:assert/strict'
import { inspectOtaFile } from '../../web/src/lib/ota.js'

function image(project = 'deskwong', version = '0.0.15') {
  const bytes = new Uint8Array(4096)
  const view = new DataView(bytes.buffer)
  bytes[0] = 0xe9
  view.setUint32(32, 0xabcd5432, true)
  new TextEncoder().encodeInto(version, bytes.subarray(48, 80))
  new TextEncoder().encodeInto(project, bytes.subarray(80, 112))
  return { size: bytes.length, slice: (a, b) => new Blob([bytes.slice(a, b)]) }
}

assert.deepEqual(await inspectOtaFile(image()), {
  project: 'deskwong', version: '0.0.15', size: 4096,
})
await assert.rejects(() => inspectOtaFile(image('bootloader')), /不是 deskwong/)
const merged = image()
merged.slice = (a, b) => new Blob([new Uint8Array(b - a)])
await assert.rejects(() => inspectOtaFile(merged), /factory/)
console.log('Web OTA image inspection passed')

