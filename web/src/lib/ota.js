const APP_DESC_OFFSET = 24 + 8
const APP_DESC_MAGIC = 0xabcd5432
const OTA_MAX_SIZE = 0x400000

function cString(bytes, offset, length) {
  const end = bytes.indexOf(0, offset)
  const stop = end >= offset && end < offset + length ? end : offset + length
  return new TextDecoder().decode(bytes.subarray(offset, stop))
}

export async function inspectOtaFile(file) {
  if (!file || file.size < APP_DESC_OFFSET + 256 || file.size > OTA_MAX_SIZE)
    throw new Error('文件大小不符合 deskwong OTA 应用镜像')
  const bytes = new Uint8Array(await file.slice(0, APP_DESC_OFFSET + 256).arrayBuffer())
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  if (bytes[0] !== 0xe9 || view.getUint32(APP_DESC_OFFSET, true) !== APP_DESC_MAGIC)
    throw new Error('不是有效的 ESP32-S3 应用镜像，请勿上传 factory 合并包')
  const version = cString(bytes, APP_DESC_OFFSET + 16, 32)
  const project = cString(bytes, APP_DESC_OFFSET + 48, 32)
  if (project !== 'deskwong' || !version)
    throw new Error('不是 deskwong OTA 应用镜像，请选择 deskwong-ota-v*.bin')
  return { project, version, size: file.size }
}

