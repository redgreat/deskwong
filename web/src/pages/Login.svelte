<script>
  import { api, setToken } from '../lib/api.js'
  export let onLogin
  let user = 'admin'
  let pass = ''
  let error = ''
  let loading = false

  async function submit() {
    loading = true
    error = ''
    try {
      const r = await api.login(user, pass)
      setToken(r.token)
      onLogin()
    } catch (e) {
      error = e.message || '登录失败'
    }
    loading = false
  }
</script>

<div class="login-wrap">
  <div class="login-card">
    <h1>deskwong</h1>
    <p class="sub">桌面摆件配置后台</p>
    <form on:submit|preventDefault={submit}>
      <label>账号
        <input type="text" bind:value={user} autocomplete="username" />
      </label>
      <label>密码
        <input type="password" bind:value={pass} autocomplete="current-password" />
      </label>
      {#if error}<p class="err">{error}</p>{/if}
      <button type="submit" disabled={loading}>{loading ? '登录中...' : '登录'}</button>
    </form>
  </div>
</div>

<style>
  .login-wrap {
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .login-card {
    background: var(--card);
    padding: 40px 36px;
    border-radius: 14px;
    box-shadow: 0 6px 30px rgba(0,0,0,.08);
    width: 320px;
  }
  h1 { margin: 0; font-size: 28px; }
  .sub { color: var(--muted); margin: 6px 0 24px; }
  label { display: block; margin-bottom: 14px; font-size: 13px; color: var(--muted); }
  input {
    display: block; width: 100%; margin-top: 6px; padding: 10px 12px;
    border: 1px solid var(--border); border-radius: 8px; font-size: 15px;
  }
  button {
    width: 100%; margin-top: 8px; padding: 11px; border: none; border-radius: 8px;
    background: var(--accent); color: #fff; font-size: 15px; cursor: pointer;
  }
  button:disabled { opacity: .6; cursor: default; }
  .err { color: var(--danger); font-size: 13px; }
</style>
