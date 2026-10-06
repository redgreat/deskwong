<script>
  import { onMount } from 'svelte'
  import { getToken, api } from './lib/api.js'
  import Login from './pages/Login.svelte'
  import Dashboard from './pages/Dashboard.svelte'

  let authed = !!getToken()

  // 启动即校验本地 token：设备每次重启都会换 token，失效就回登录页，
  // 不等用户在配置页点出报错（api 层遇 401 也会清 token 并刷新页面）
  onMount(async () => {
    if (!authed) return
    try {
      await api.getConfig()
    } catch {
      authed = !!getToken()
    }
  })

  function onLogin() { authed = true }
  function onLogout() { authed = false }
</script>

{#if authed}
  <Dashboard onLogout={onLogout} />
{:else}
  <Login onLogin={onLogin} />
{/if}
