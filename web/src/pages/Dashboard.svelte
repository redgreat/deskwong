<script>
  import { api, setToken } from '../lib/api.js'
  export let onLogout

  let loading = false
  let saved = false
  let error = ''
  let health = null
  let cfg = {
    wifi_ssid: '', wifi_pass: '',
    timezone: 'Asia/Shanghai', admin_user: 'admin', admin_pass: '',
    weather_location: '', weather_key: '',
    worktime_api_base: '', worktime_token: '',
    aiusage_api_base: '', aiusage_token: '',
    mqtt_broker: '', mqtt_port: 1883, mqtt_user: '', mqtt_pass: '',
    remind_signin_hh: 8, remind_signin_mm: 25,
    remind_signout_hh: 17, remind_signout_mm: 35,
    remind_worktime_hh: 17, remind_worktime_mm: 0,
    remind_enabled: true
  }

  async function load() {
    try {
      const r = await api.getConfig()
      cfg = { ...cfg, ...r.data }
      // 脱敏字段回填占位
      if (cfg.wifi_pass === '******') cfg.wifi_pass = ''
      if (cfg.admin_pass === '******') cfg.admin_pass = ''
      if (cfg.weather_key === '******') cfg.weather_key = ''
      if (cfg.worktime_token === '******') cfg.worktime_token = ''
      if (cfg.aiusage_token === '******') cfg.aiusage_token = ''
      if (cfg.mqtt_pass === '******') cfg.mqtt_pass = ''
    } catch (e) {
      error = e.message
    }
  }

  async function refreshHealth() {
    try { health = (await api.health()).data } catch (e) { health = null }
  }

  async function save() {
    loading = true
    error = ''
    saved = false
    try {
      await api.saveConfig(cfg)
      saved = true
      setTimeout(() => (saved = false), 2500)
    } catch (e) {
      error = e.message
    }
    loading = false
  }

  function logout() {
    setToken('')
    onLogout()
  }

  async function doRestart() {
    if (confirm('确定重启设备？')) {
      try { await api.restart() } catch (e) { error = e.message }
    }
  }

  async function doFactoryReset() {
    if (confirm('确定恢复出厂设置？所有配置将清空！')) {
      try { await api.factoryReset() } catch (e) { error = e.message }
    }
  }

  let otaFile = null
  let otaProgress = false
  async function onOtaChange(e) {
    otaFile = e.target.files[0]
  }
  async function doOta() {
    if (!otaFile) { error = '请先选择固件文件'; return }
    if (!confirm(`确定上传 ${otaFile.name} 升级？设备将自动重启。`)) return
    otaProgress = true
    error = ''
    try {
      await api.uploadOta(otaFile)
    } catch (e) {
      error = e.message
    }
    otaProgress = false
  }

  load()
  refreshHealth()
  setInterval(refreshHealth, 8000)
</script>

<div class="wrap">
  <header>
    <div>
      <h1>deskwong 配置后台</h1>
      {#if health}
        <span class="badge {health.wifi_connected ? 'ok' : 'warn'}">
          {health.wifi_connected ? 'WiFi 已连接' : (health.ap_mode ? 'AP 配置模式' : 'WiFi 未连接')}
        </span>
        <span class="ip">{health.ip}</span>
      {/if}
    </div>
    <button class="ghost" on:click={logout}>退出登录</button>
  </header>

  {#if error}<p class="err">{error}</p>{/if}

  <form on:submit|preventDefault={save}>
    <section>
      <h2>网络</h2>
      <label>WiFi SSID<input bind:value={cfg.wifi_ssid} /></label>
      <label>WiFi 密码<input type="password" bind:value={cfg.wifi_pass} placeholder="留空不修改" /></label>
    </section>

    <section>
      <h2>系统</h2>
      <label>时区<input bind:value={cfg.timezone} placeholder="Asia/Shanghai" /></label>
      <label>管理员账号<input bind:value={cfg.admin_user} /></label>
      <label>管理员密码<input type="password" bind:value={cfg.admin_pass} placeholder="留空不修改" /></label>
    </section>

    <section>
      <h2>天气</h2>
      <label>位置（城市/经纬度）<input bind:value={cfg.weather_location} /></label>
      <label>和风天气 Key<input bind:value={cfg.weather_key} placeholder="留空不修改" /></label>
    </section>

    <section>
      <h2>工时（PingCode）</h2>
      <label>API 地址<input bind:value={cfg.worktime_api_base} placeholder="http://host/worktime" /></label>
      <label>Token<input bind:value={cfg.worktime_token} placeholder="留空不修改" /></label>
    </section>

    <section>
      <h2>AI 用量</h2>
      <label>聚合 API 地址<input bind:value={cfg.aiusage_api_base} /></label>
      <label>Token<input bind:value={cfg.aiusage_token} placeholder="留空不修改" /></label>
    </section>

    <section>
      <h2>RaceBox（MQTT）</h2>
      <label>Broker 地址<input bind:value={cfg.mqtt_broker} /></label>
      <label>端口<input type="number" bind:value={cfg.mqtt_port} /></label>
      <label>用户名<input bind:value={cfg.mqtt_user} /></label>
      <label>密码<input type="password" bind:value={cfg.mqtt_pass} placeholder="留空不修改" /></label>
    </section>

    <section>
      <h2>提醒</h2>
      <label class="inline"><input type="checkbox" bind:checked={cfg.remind_enabled} /> 启用提醒</label>
      <div class="row">
        <label>签到提醒
          <input type="number" bind:value={cfg.remind_signin_hh} min="0" max="23" /> :
          <input type="number" bind:value={cfg.remind_signin_mm} min="0" max="59" />
        </label>
        <label>签退提醒
          <input type="number" bind:value={cfg.remind_signout_hh} min="0" max="23" /> :
          <input type="number" bind:value={cfg.remind_signout_mm} min="0" max="59" />
        </label>
        <label>记录工时提醒
          <input type="number" bind:value={cfg.remind_worktime_hh} min="0" max="23" /> :
          <input type="number" bind:value={cfg.remind_worktime_mm} min="0" max="59" />
        </label>
      </div>
    </section>

    <div class="actions">
      <button type="submit" disabled={loading}>{loading ? '保存中...' : '保存配置'}</button>
      {#if saved}<span class="ok">已保存</span>{/if}
    </div>
  </form>

  <section class="sysops">
    <h2>系统操作</h2>
    <div class="sysops-row">
      <button class="ghost" on:click={doRestart}>重启设备</button>
      <button class="danger" on:click={doFactoryReset}>恢复出厂设置</button>
    </div>
    <div class="sysops-row ota-row">
      <input type="file" accept=".bin" on:change={onOtaChange} />
      <button class="ghost" on:click={doOta} disabled={otaProgress || !otaFile}>
        {otaProgress ? '上传中...' : 'OTA 升级'}
      </button>
    </div>
  </section>
</div>

<style>
  .wrap { max-width: 760px; margin: 0 auto; padding: 24px; }
  header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
  h1 { font-size: 22px; margin: 0; }
  h2 { font-size: 16px; margin: 0 0 12px; color: var(--accent); }
  .badge { font-size: 12px; padding: 3px 8px; border-radius: 999px; margin-left: 10px; }
  .badge.ok { background: #dcfce7; color: #15803d; }
  .badge.warn { background: #fef3c7; color: #b45309; }
  .ip { color: var(--muted); font-size: 13px; margin-left: 10px; }
  .ghost { background: transparent; border: 1px solid var(--border); border-radius: 8px; padding: 7px 14px; cursor: pointer; }
  section { background: var(--card); border-radius: 12px; padding: 18px; margin-bottom: 16px; box-shadow: 0 1px 4px rgba(0,0,0,.05); }
  label { display: block; font-size: 13px; color: var(--muted); margin-bottom: 12px; }
  label.inline { display: flex; align-items: center; gap: 6px; color: var(--text); }
  input { display: block; width: 100%; margin-top: 6px; padding: 9px 12px; border: 1px solid var(--border); border-radius: 8px; font-size: 14px; }
  .row { display: flex; gap: 16px; flex-wrap: wrap; }
  .row label { flex: 1; min-width: 150px; }
  .row input { display: inline-block; width: 48px; margin: 0 2px; padding: 6px; }
  .actions { display: flex; align-items: center; gap: 14px; }
  .actions button { padding: 11px 26px; border: none; border-radius: 8px; background: var(--accent); color: #fff; font-size: 15px; cursor: pointer; }
  .sysops { margin-top: 4px; }
  .sysops-row { display: flex; gap: 12px; align-items: center; }
  .sysops-row button { padding: 9px 18px; border-radius: 8px; cursor: pointer; }
  .sysops-row .ghost { background: transparent; border: 1px solid var(--border); color: var(--text); }
  .sysops-row .danger { background: #fef2f2; border: 1px solid #fecaca; color: var(--danger); }
  .ota-row { margin-top: 10px; }
  .ota-row input[type=file] { font-size: 13px; }
  .ok { color: #15803d; font-size: 14px; }
  .err { color: var(--danger); font-size: 13px; margin-bottom: 12px; }
</style>
