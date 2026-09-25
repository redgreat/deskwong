<script>
  import { api, setToken } from '../lib/api.js'
  export let onLogout

  let loading = false
  let saved = false
  let error = ''
  let health = null
  let dark = localStorage.getItem('theme') === 'dark'
  let cfg = {
    wifi_ssid: '', wifi_pass: '',
    timezone: 'Asia/Shanghai', admin_user: 'admin', admin_pass: '',
    weather_location: '', weather_api_url: '',
    weather_key: '', weather_refresh_minutes: 30,
    worktime_api_base: '', worktime_token: '', worktime_refresh_minutes: 30,
    aiusage_api_base: '', aiusage_token: '', aiusage_refresh_minutes: 10,
    mqtt_broker: '', mqtt_port: 1883, mqtt_user: '', mqtt_pass: '',
    racebox_upload_topic: 'deskwong/racebox/data', racebox_device_name: 'RaceBox',
    racebox_device_lock: '',
    racebox_auto_erase: false,
    remind_signin_hh: 8, remind_signin_mm: 25,
    remind_signout_hh: 17, remind_signout_mm: 35,
    remind_worktime_hh: 17, remind_worktime_mm: 0,
    remind_enabled: true,
    voice_enabled: false, voice_wake_word: '你好小智',
    voice_server_url: '', voice_token: '', voice_device_id: '',
    voice_volume: 60, voice_listen_mode: 0, voice_aec_level: 2,
    voice_reply_seconds: 30, voice_tts_scroll: true, voice_mcp_enabled: true
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
      if (cfg.voice_token === '******') cfg.voice_token = ''
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
      // Keep the restart reminder visible until the next edit/save.
    } catch (e) {
      error = e.message
    }
    loading = false
  }

  function applyTheme() {
    document.documentElement.dataset.theme = dark ? 'dark' : 'light'
    document.documentElement.style.colorScheme = dark ? 'dark' : 'light'
    localStorage.setItem('theme', dark ? 'dark' : 'light')
  }

  function toggleTheme() {
    dark = !dark
    applyTheme()
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

  applyTheme()
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
    <div class="header-actions">
      <button class="icon-button" type="button" on:click={toggleTheme} aria-label={dark ? '切换浅色主题' : '切换暗黑主题'} title={dark ? '切换浅色主题' : '切换暗黑主题'}>
        <span aria-hidden="true">{dark ? '☾' : '☀'}</span>
      </button>
      <button class="button secondary compact" type="button" on:click={logout}>退出登录</button>
    </div>
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
      <p class="section-help">
        默认走 <strong>Open-Meteo</strong>（免费、无需注册、无需 Key）：位置填「纬度,经度」即可，例如 <code>36.07,120.38</code>。
        如果需要国内分钟级实况，再填和风天气的专属 API Host 与 Key（免费版每天约 1000~2000 次，本项目一天只请求几十次，完全够用）。
        设备仅在 WiFi 已联网时请求，最低刷新周期 5 分钟。
      </p>
      <label>位置（经纬度，或和风 LocationID）<input bind:value={cfg.weather_location} placeholder="例如 36.07,120.38 或 101120201" /></label>
      <label>实时天气请求地址（留空用 Open-Meteo 默认地址）<input bind:value={cfg.weather_api_url} placeholder="https://api.open-meteo.com/v1/forecast 或 https://你的API-Host/v7/weather/now" /></label>
      <label>和风天气 Key（留空 = 使用免密钥 Open-Meteo）<input bind:value={cfg.weather_key} placeholder="留空不修改" /></label>
      <label>天气刷新频率（分钟）<input type="number" bind:value={cfg.weather_refresh_minutes} min="5" max="1440" /></label>
    </section>

    <section>
      <h2>工时（PingCode）</h2>
      <label>API 地址<input bind:value={cfg.worktime_api_base} placeholder="http://host/worktime" /></label>
      <p class="muted">工时 MySQL、工号和 SQL 在工时服务地址的 /admin 页面配置，保存到 SQLite，重启服务生效。</p>
      <label>Token<input bind:value={cfg.worktime_token} placeholder="留空不修改" /></label>
      <label>工时记录刷新频率（分钟）<input type="number" bind:value={cfg.worktime_refresh_minutes} min="5" max="1440" /></label>
    </section>

    <section>
      <h2>AI 用量</h2>
      <p class="section-help">ChatGPT/Codex 的 5 小时和每周订阅额度没有官方公开接口，不能直接填写 OpenAI API Key。这里连接你自己部署的聚合服务。</p>
      <label>聚合 API 地址<input bind:value={cfg.aiusage_api_base} placeholder="例如 http://20.20.10.92:8001" /></label>
      <label>Token<input bind:value={cfg.aiusage_token} placeholder="留空不修改" /></label>
      <label>AI 用量刷新频率（分钟）<input type="number" bind:value={cfg.aiusage_refresh_minutes} min="1" max="1440" /></label>
      <details class="api-help">
        <summary>接口格式说明</summary>
        <p>设备请求 <code>GET 地址/ai/usage</code>，Token 通过 Bearer 鉴权发送。服务应返回 5 小时和每周两个窗口。</p>
        <code class="code-block">&#123;"data":&#123;"providers":[&#123;"id":"chatgpt_5h","window_minutes":300,"remaining_percent":72,"resets_at":1789223400&#125;,&#123;"id":"chatgpt_weekly","window_minutes":10080,"remaining_percent":44,"resets_at":1789741800&#125;]&#125;&#125;</code>
      </details>
    </section>

    <section>
      <h2>RaceBox（MQTT）</h2>
      {#if health?.mqtt_message}
        <p class="muted">设备状态：{health.mqtt_message}</p>
      {/if}
      <label>Broker 地址<input bind:value={cfg.mqtt_broker} placeholder="例如 mqtt.example.com 或 mqtts://mqtt.example.com:8883" /></label>
      <p class="muted">保存时设备会实际连接 Broker 并校验地址、端口和账号密码；校验失败不会覆盖原配置。保存成功后重启设备生效。</p>
      <label>端口<input type="number" bind:value={cfg.mqtt_port} /></label>
      <label>用户名<input bind:value={cfg.mqtt_user} /></label>
      <label>密码<input type="password" bind:value={cfg.mqtt_pass} placeholder="留空不修改" /></label>
      <label>上传主题 / 地址<input bind:value={cfg.racebox_upload_topic} placeholder="deskwong/racebox/data" /></label>
      <label>蓝牙设备名前缀<input bind:value={cfg.racebox_device_name} placeholder="RaceBox" /></label>
      <label>锁定设备名（可选，填了就只连这一台）<input bind:value={cfg.racebox_device_lock} placeholder="例如 RaceBox Mini S/N 12345，留空则按前缀搜索" /></label>
      <div class="setting-row">
        <div>
          <strong>上传后清除内存</strong>
          <span>默认关闭；测试期间保留设备数据，可重复下载</span>
        </div>
        <label class="switch" aria-label="上传成功后清除 RaceBox 内存">
          <input type="checkbox" bind:checked={cfg.racebox_auto_erase} />
          <span class="switch-track"><span class="switch-thumb"></span></span>
        </label>
      </div>
    </section>

    <section>
      <h2>提醒</h2>
      <div class="setting-row reminder-master">
        <div><strong>启用提醒</strong><span>按下方时间在设备上提示</span></div>
        <label class="switch" aria-label="启用提醒">
          <input type="checkbox" bind:checked={cfg.remind_enabled} />
          <span class="switch-track"><span class="switch-thumb"></span></span>
        </label>
      </div>
      <div class="reminder-grid" class:disabled={!cfg.remind_enabled}>
        <label class="time-card"><span>签到提醒</span><div class="time-input"><input type="number" bind:value={cfg.remind_signin_hh} min="0" max="23" disabled={!cfg.remind_enabled} /><b>:</b><input type="number" bind:value={cfg.remind_signin_mm} min="0" max="59" disabled={!cfg.remind_enabled} /></div></label>
        <label class="time-card"><span>签退提醒</span><div class="time-input"><input type="number" bind:value={cfg.remind_signout_hh} min="0" max="23" disabled={!cfg.remind_enabled} /><b>:</b><input type="number" bind:value={cfg.remind_signout_mm} min="0" max="59" disabled={!cfg.remind_enabled} /></div></label>
        <label class="time-card"><span>记录工时</span><div class="time-input"><input type="number" bind:value={cfg.remind_worktime_hh} min="0" max="23" disabled={!cfg.remind_enabled} /><b>:</b><input type="number" bind:value={cfg.remind_worktime_mm} min="0" max="59" disabled={!cfg.remind_enabled} /></div></label>
      </div>
    </section>

    <section>
      <h2>语音（小智）</h2>
      <p class="section-help">
        接入 <strong>xiaozhi-esp32</strong>：唤醒后由服务端识别意图，通过 MCP 调用设备工具
        （翻月、刷新天气/AI/工时、同步 RaceBox）。协议层尚未内置，这里先把配置就位。
      </p>
      <div class="setting-row voice-master">
        <div><strong>启用语音</strong><span>需要服务端已部署 xiaozhi-server</span></div>
        <label class="switch" aria-label="启用语音">
          <input type="checkbox" bind:checked={cfg.voice_enabled} />
          <span class="switch-track"><span class="switch-thumb"></span></span>
        </label>
      </div>
      <label>服务端地址<input bind:value={cfg.voice_server_url} placeholder="wss://xiaozhi.example.com/ws" disabled={!cfg.voice_enabled} /></label>
      <label>设备 Token<input bind:value={cfg.voice_token} placeholder="留空不修改" disabled={!cfg.voice_enabled} /></label>
      <label>设备 ID / 激活码<input bind:value={cfg.voice_device_id} disabled={!cfg.voice_enabled} /></label>
      <label>唤醒词<input bind:value={cfg.voice_wake_word} disabled={!cfg.voice_enabled} /></label>
      <label>监听方式
        <select bind:value={cfg.voice_listen_mode} disabled={!cfg.voice_enabled}>
          <option value={0}>唤醒词</option>
          <option value={1}>长按 KEY</option>
          <option value={2}>两者皆可</option>
        </select>
      </label>
      <label>音量（0-100）<input type="number" bind:value={cfg.voice_volume} min="0" max="100" disabled={!cfg.voice_enabled} /></label>
      <label>回声消除强度（0-3）<input type="number" bind:value={cfg.voice_aec_level} min="0" max="3" disabled={!cfg.voice_enabled} /></label>
      <label>单次对话最长（秒）<input type="number" bind:value={cfg.voice_reply_seconds} min="5" max="120" disabled={!cfg.voice_enabled} /></label>
      <div class="setting-row">
        <div><strong>回答文字滚动显示</strong><span>在屏幕底部滚动显示 TTS 文本</span></div>
        <label class="switch" aria-label="回答文字滚动显示">
          <input type="checkbox" bind:checked={cfg.voice_tts_scroll} disabled={!cfg.voice_enabled} />
          <span class="switch-track"><span class="switch-thumb"></span></span>
        </label>
      </div>
      <div class="setting-row">
        <div><strong>允许 MCP 控制设备</strong><span>服务端可调用翻月、刷新、同步等工具</span></div>
        <label class="switch" aria-label="允许 MCP 控制设备">
          <input type="checkbox" bind:checked={cfg.voice_mcp_enabled} disabled={!cfg.voice_enabled} />
          <span class="switch-track"><span class="switch-thumb"></span></span>
        </label>
      </div>
    </section>

    <p class="section-help">
      天气 / 工时 / AI 用量三个定时拉取由设备内同一个调度器<strong>排队串行</strong>执行：到期的任务只是入队，
      一次只跑一个，互不抢占网络；修改上面的频率会立即生效，无需重启。
    </p>

    <div class="actions">
      <button class="button primary" type="submit" disabled={loading}>{loading ? '保存中...' : '保存配置'}</button>
      {#if saved}<span class="ok">已保存到设备 Flash，断电不丢失；重启设备后生效</span>{/if}
    </div>
  </form>

  <section class="sysops">
    <h2>系统操作</h2>
    <div class="sysops-row">
      <button class="button secondary" type="button" on:click={doRestart}>重启设备</button>
      <button class="button danger" type="button" on:click={doFactoryReset}>恢复出厂设置</button>
    </div>
    <div class="sysops-row ota-row">
      <input type="file" accept=".bin" on:change={onOtaChange} />
      <button class="button secondary" type="button" on:click={doOta} disabled={otaProgress || !otaFile}>
        {otaProgress ? '上传中...' : 'OTA 升级'}
      </button>
    </div>
  </section>
</div>

<style>
  .wrap { max-width: 800px; margin: 0 auto; padding: 28px 22px 48px; }
  header { display: flex; justify-content: space-between; align-items: center; gap: 18px; margin-bottom: 22px; }
  .header-actions { display: flex; align-items: center; gap: 10px; }
  h1 { font-size: 23px; margin: 0 0 7px; letter-spacing: -.02em; }
  h2 { font-size: 16px; margin: 0 0 16px; color: var(--accent); }
  .section-help { color: var(--muted); font-size: 13px; line-height: 1.65; margin: -7px 0 16px; }
  .api-help { margin-top: 2px; padding: 12px 14px; border: 1px solid var(--border); border-radius: 11px; background: var(--surface-soft); color: var(--muted); font-size: 12px; }
  .api-help summary { color: var(--accent); font-weight: 600; cursor: pointer; }
  .api-help p { line-height: 1.6; margin: 10px 0; }
  .api-help code { color: var(--text); }
  .code-block { display: block; padding: 10px; border-radius: 8px; background: var(--input); overflow-wrap: anywhere; line-height: 1.5; }
  .badge { display: inline-flex; align-items: center; font-size: 12px; padding: 4px 9px; border-radius: 999px; }
  .badge.ok { background: var(--success-soft); color: var(--success); }
  .badge.warn { background: var(--warning-soft); color: var(--warning); }
  .ip { color: var(--muted); font-size: 13px; margin-left: 10px; }
  .icon-button { width: 38px; height: 38px; display: grid; place-items: center; padding: 0; border: 1px solid var(--border); border-radius: 11px; background: var(--card); color: var(--text); box-shadow: var(--shadow-sm); cursor: pointer; font-size: 19px; transition: transform .16s, border-color .16s, background .16s; }
  .icon-button:hover { transform: translateY(-1px); border-color: var(--accent); background: var(--accent-soft); }
  section { background: var(--card); border: 1px solid var(--border-subtle); border-radius: 16px; padding: 20px; margin-bottom: 16px; box-shadow: var(--shadow); transition: background .2s, border-color .2s; }
  label { display: block; font-size: 13px; color: var(--muted); margin-bottom: 14px; }
  input { display: block; width: 100%; margin-top: 7px; padding: 10px 12px; border: 1px solid var(--border); border-radius: 10px; background: var(--input); color: var(--text); font: inherit; font-size: 14px; outline: none; transition: border-color .16s, box-shadow .16s, background .2s; }
  input:focus { border-color: var(--accent); box-shadow: 0 0 0 3px var(--focus); }
  input:disabled { cursor: not-allowed; opacity: .55; }
  .button { min-height: 40px; padding: 9px 17px; border: 1px solid transparent; border-radius: 10px; font: inherit; font-size: 14px; font-weight: 600; cursor: pointer; transition: transform .16s, box-shadow .16s, background .16s, border-color .16s; }
  .button:hover:not(:disabled) { transform: translateY(-1px); }
  .button:disabled { cursor: not-allowed; opacity: .5; }
  .button.primary { color: white; background: var(--accent); box-shadow: 0 5px 14px var(--accent-shadow); }
  .button.primary:hover:not(:disabled) { background: var(--accent-hover); }
  .button.secondary { color: var(--text); background: var(--button-soft); border-color: var(--border); }
  .button.secondary:hover:not(:disabled) { border-color: var(--accent); background: var(--accent-soft); }
  .button.danger { color: var(--danger); background: var(--danger-soft); border-color: var(--danger-border); }
  .button.compact { min-height: 38px; padding: 8px 14px; }
  .setting-row { display: flex; align-items: center; justify-content: space-between; gap: 20px; padding: 15px 16px; border: 1px solid var(--border); border-radius: 12px; background: var(--surface-soft); }
  .setting-row strong { display: block; color: var(--text); font-size: 14px; margin-bottom: 4px; }
  .setting-row span { display: block; color: var(--muted); font-size: 12px; line-height: 1.5; }
  .switch { flex: 0 0 auto; display: block; margin: 0; cursor: pointer; }
  .switch input { position: absolute; opacity: 0; pointer-events: none; }
  .switch-track { position: relative; display: block; width: 46px; height: 26px; border-radius: 999px; background: var(--switch-off); transition: background .2s, box-shadow .2s; }
  .switch-thumb { position: absolute; top: 3px; left: 3px; width: 20px; height: 20px; border-radius: 50%; background: #fff; box-shadow: 0 2px 5px rgba(0,0,0,.25); transition: transform .2s; }
  .switch input:checked + .switch-track { background: var(--accent); }
  .switch input:checked + .switch-track .switch-thumb { transform: translateX(20px); }
  .switch input:focus-visible + .switch-track { box-shadow: 0 0 0 3px var(--focus); }
  select { display: block; width: 100%; margin-top: 7px; padding: 10px 12px; border: 1px solid var(--border); border-radius: 10px; background: var(--input); color: var(--text); font: inherit; font-size: 14px; outline: none; transition: border-color .16s, box-shadow .16s; }
  select:focus { border-color: var(--accent); box-shadow: 0 0 0 3px var(--focus); }
  select:disabled { cursor: not-allowed; opacity: .55; }
  .reminder-master, .voice-master { margin-bottom: 14px; }
  .reminder-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; transition: opacity .2s; }
  .reminder-grid.disabled { opacity: .58; }
  .time-card { margin: 0; padding: 14px; border: 1px solid var(--border); border-radius: 12px; background: var(--surface-soft); color: var(--text); }
  .time-card > span { display: block; margin-bottom: 10px; font-size: 13px; font-weight: 600; }
  .time-input { display: flex; align-items: center; gap: 6px; }
  .time-input input { width: 58px; min-width: 0; margin: 0; padding: 8px 4px; text-align: center; font-weight: 600; }
  .time-input b { color: var(--muted); }
  .actions { display: flex; align-items: center; gap: 14px; margin: 2px 0 18px; }
  .sysops { margin-top: 4px; }
  .sysops-row { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
  .ota-row { margin-top: 14px; padding-top: 14px; border-top: 1px solid var(--border-subtle); }
  .ota-row input[type=file] { flex: 1 1 320px; margin: 0; font-size: 13px; }
  .ok { color: var(--success); font-size: 14px; }
  .err { color: var(--danger); font-size: 13px; margin-bottom: 12px; }
  @media (max-width: 650px) {
    .wrap { padding: 18px 12px 36px; }
    header { align-items: flex-start; }
    .reminder-grid { grid-template-columns: 1fr; }
    .time-input input { width: 72px; }
  }
</style>
