// API 封装：设备内嵌 HTTP 服务
let token = localStorage.getItem('token') || ''

export function setToken(t) {
  token = t || ''
  if (t) localStorage.setItem('token', t)
  else localStorage.removeItem('token')
}

export function getToken() {
  return token
}

async function req(path, method = 'GET', body) {
  const headers = {}
  if (token) headers['Authorization'] = 'Bearer ' + token
  if (body) headers['Content-Type'] = 'application/json'
  const res = await fetch(path, {
    method,
    headers,
    body: body ? JSON.stringify(body) : undefined
  })
  const data = await res.json().catch(() => ({}))
  if (!res.ok) {
    // 凭据过期/无效：清掉本地 token 回登录页，避免停在报错的页面
    if (res.status === 401 && path !== '/api/auth/login') {
      setToken('')
      location.reload()
      throw new Error('登录已过期，请重新登录')
    }
    throw new Error(data.message || res.statusText)
  }
  if (data.code !== 0 && data.code !== undefined) throw new Error(data.message || 'error')
  return data
}

export const api = {
  health: () => req('/api/health'),
  login: (user, pass) => req('/api/auth/login', 'POST', { user, pass }),
  getConfig: () => req('/api/config'),
  saveConfig: (cfg) => req('/api/config', 'PUT', cfg),
  restart: () => req('/api/system/restart', 'POST'),
  factoryReset: () => req('/api/system/factory-reset', 'POST'),
  logs: () => req('/api/system/logs'),
  uploadOta: async (file) => {
    const res = await fetch('/api/ota', {
      method: 'POST',
      headers: { 'Authorization': 'Bearer ' + token, 'Content-Type': 'application/octet-stream' },
      body: file
    })
    const data = await res.json().catch(() => ({}))
    if (!res.ok) {
      if (res.status === 401) {
        setToken('')
        location.reload()
        throw new Error('登录已过期，请重新登录')
      }
      throw new Error(data.message || res.statusText)
    }
    return data
  }
}
