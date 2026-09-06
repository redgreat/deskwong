<script>
  import { getToken } from './lib/api.js'
  import Login from './pages/Login.svelte'
  import Dashboard from './pages/Dashboard.svelte'

  let authed = !!getToken()
  function onLogin() { authed = true }
  function onLogout() { authed = false }
</script>

{#if authed}
  <Dashboard onLogout={onLogout} />
{:else}
  <Login onLogin={onLogin} />
{/if}
