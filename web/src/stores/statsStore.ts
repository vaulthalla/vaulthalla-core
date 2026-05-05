import { create } from 'zustand'
import { WSCommandPayload } from '@/util/webSocketCommands'
import { useWebSocketStore } from '@/stores/useWebSocket'
import { VaultStats } from '@/models/stats/vaultStats'
import { VaultActivity } from '@/models/stats/vaultActivity'
import { VaultRecovery } from '@/models/stats/vaultRecovery'
import { VaultSecurity } from '@/models/stats/vaultSecurity'
import { VaultShareStats } from '@/models/stats/vaultShareStats'
import { VaultSyncHealth } from '@/models/stats/vaultSyncHealth'
import { CacheStats } from '@/models/stats/cacheStats'
import { DbStats } from '@/models/stats/dbStats'
import { FuseStats } from '@/models/stats/fuseStats'
import { SystemHealth } from '@/models/stats/systemHealth'
import { ThreadPoolManagerStats } from '@/models/stats/threadPoolStats'
import { getErrorMessage } from '@/util/handleErrors'

/**
 * Generic wrapper that carries:
 * - the actual stats payload
 * - UI metadata (loading/error/lastUpdated)
 * - optional polling handle
 */
export interface StatsWrapper<T> {
  data: T
  lastUpdated: number | null
  loading: boolean
  error: string | null

  // Optional extras you may want later:
  lastSuccessAt?: number | null
  lastErrorAt?: number | null
}

export type CacheStatsWrapper = StatsWrapper<CacheStats>
export type SystemHealthWrapper = StatsWrapper<SystemHealth>
export type ThreadPoolStatsWrapper = StatsWrapper<ThreadPoolManagerStats>
export type FuseStatsWrapper = StatsWrapper<FuseStats>
export type DbStatsWrapper = StatsWrapper<DbStats>

const makeCacheWrapper = (initial?: Partial<CacheStatsWrapper>): CacheStatsWrapper => ({
  data: new CacheStats({}),
  lastUpdated: null,
  loading: false,
  error: null,
  lastSuccessAt: null,
  lastErrorAt: null,
  ...initial,
})

const makeSystemHealthWrapper = (initial?: Partial<SystemHealthWrapper>): SystemHealthWrapper => ({
  data: new SystemHealth(),
  lastUpdated: null,
  loading: false,
  error: null,
  lastSuccessAt: null,
  lastErrorAt: null,
  ...initial,
})

const makeThreadPoolStatsWrapper = (initial?: Partial<ThreadPoolStatsWrapper>): ThreadPoolStatsWrapper => ({
  data: new ThreadPoolManagerStats(),
  lastUpdated: null,
  loading: false,
  error: null,
  lastSuccessAt: null,
  lastErrorAt: null,
  ...initial,
})

const makeFuseStatsWrapper = (initial?: Partial<FuseStatsWrapper>): FuseStatsWrapper => ({
  data: new FuseStats(),
  lastUpdated: null,
  loading: false,
  error: null,
  lastSuccessAt: null,
  lastErrorAt: null,
  ...initial,
})

const makeDbStatsWrapper = (initial?: Partial<DbStatsWrapper>): DbStatsWrapper => ({
  data: new DbStats(),
  lastUpdated: null,
  loading: false,
  error: null,
  lastSuccessAt: null,
  lastErrorAt: null,
  ...initial,
})

interface StatsStore {
  // Two independent caches, same type
  fsCacheStats: CacheStatsWrapper
  httpCacheStats: CacheStatsWrapper
  systemHealth: SystemHealthWrapper
  threadPoolStats: ThreadPoolStatsWrapper
  fuseStats: FuseStatsWrapper
  dbStats: DbStatsWrapper

  // Fetchers (raw RPC)
  getVaultStats: (payload: WSCommandPayload<'stats.vault'>) => Promise<VaultStats>
  getVaultSyncHealth: (payload: WSCommandPayload<'stats.vault.sync'>) => Promise<VaultSyncHealth>
  getVaultActivity: (payload: WSCommandPayload<'stats.vault.activity'>) => Promise<VaultActivity>
  getVaultShareStats: (payload: WSCommandPayload<'stats.vault.shares'>) => Promise<VaultShareStats>
  getVaultRecovery: (payload: WSCommandPayload<'stats.vault.recovery'>) => Promise<VaultRecovery>
  getVaultSecurity: (payload: WSCommandPayload<'stats.vault.security'>) => Promise<VaultSecurity>
  getSystemHealth: (payload?: WSCommandPayload<'stats.system.health'>) => Promise<SystemHealth>
  getThreadPoolStats: (payload?: WSCommandPayload<'stats.system.threadpools'>) => Promise<ThreadPoolManagerStats>
  getFuseStats: (payload?: WSCommandPayload<'stats.system.fuse'>) => Promise<FuseStats>
  getDbStats: (payload?: WSCommandPayload<'stats.system.db'>) => Promise<DbStats>
  getFsCacheStats: (payload?: WSCommandPayload<'stats.fs.cache'>) => Promise<CacheStats>
  getHttpCacheStats: (payload?: WSCommandPayload<'stats.http.cache'>) => Promise<CacheStats>

  // Refresh helpers
  refreshSystemHealth: () => Promise<SystemHealth>
  refreshThreadPoolStats: () => Promise<ThreadPoolManagerStats>
  refreshFuseStats: () => Promise<FuseStats>
  refreshDbStats: () => Promise<DbStats>
  refreshFsCacheStats: () => Promise<CacheStats>
  refreshHttpCacheStats: () => Promise<CacheStats>

  // Polling (separate pollers)
  startSystemHealthPolling: (intervalMs?: number) => void
  stopSystemHealthPolling: () => void
  startThreadPoolStatsPolling: (intervalMs?: number) => void
  stopThreadPoolStatsPolling: () => void
  startFuseStatsPolling: (intervalMs?: number) => void
  stopFuseStatsPolling: () => void
  startDbStatsPolling: (intervalMs?: number) => void
  stopDbStatsPolling: () => void
  startFsCacheStatsPolling: (intervalMs?: number) => void
  stopFsCacheStatsPolling: () => void
  startHttpCacheStatsPolling: (intervalMs?: number) => void
  stopHttpCacheStatsPolling: () => void

  // internal
  _systemHealthPoller: number | null
  _threadPoolStatsPoller: number | null
  _fuseStatsPoller: number | null
  _dbStatsPoller: number | null
  _fsCachePoller: number | null
  _httpCachePoller: number | null
}

export const useStatsStore = create<StatsStore>((set, get) => ({
  fsCacheStats: makeCacheWrapper(),
  httpCacheStats: makeCacheWrapper(),
  systemHealth: makeSystemHealthWrapper(),
  threadPoolStats: makeThreadPoolStatsWrapper(),
  fuseStats: makeFuseStatsWrapper(),
  dbStats: makeDbStatsWrapper(),

  _systemHealthPoller: null,
  _threadPoolStatsPoller: null,
  _fuseStatsPoller: null,
  _dbStatsPoller: null,
  _fsCachePoller: null,
  _httpCachePoller: null,

  async getVaultStats(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault', vault_id)
    return response.stats
  },

  async getVaultSyncHealth(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault.sync', vault_id)
    return VaultSyncHealth.from(response.stats)
  },

  async getVaultActivity(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault.activity', vault_id)
    return VaultActivity.from(response.stats)
  },

  async getVaultShareStats(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault.shares', vault_id)
    return VaultShareStats.from(response.stats)
  },

  async getVaultRecovery(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault.recovery', vault_id)
    return VaultRecovery.from(response.stats)
  },

  async getVaultSecurity(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault.security', vault_id)
    return VaultSecurity.from(response.stats)
  },

  async getSystemHealth() {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.system.health', null)
    return SystemHealth.from(response.stats)
  },

  async getThreadPoolStats() {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.system.threadpools', null)
    return ThreadPoolManagerStats.from(response.stats)
  },

  async getFuseStats() {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.system.fuse', null)
    return FuseStats.from(response.stats)
  },

  async getDbStats() {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.system.db', null)
    return DbStats.from(response.stats)
  },

  async getFsCacheStats() {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.fs.cache', null)
    return response.stats
  },

  async getHttpCacheStats() {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.http.cache', null)
    return response.stats
  },

  async refreshSystemHealth() {
    const current = get().systemHealth
    if (current.loading) return current.data

    set({ systemHealth: { ...current, loading: true, error: null } })

    try {
      const health = await get().getSystemHealth()
      const nextData = SystemHealth.from(health ?? {})
      const now = Date.now()

      set({
        systemHealth: {
          ...get().systemHealth,
          data: nextData,
          lastUpdated: now,
          lastSuccessAt: now,
          loading: false,
          error: null,
        },
      })

      return nextData
    } catch (error: unknown) {
      const msg = getErrorMessage(error) || 'Failed to fetch system health'
      const now = Date.now()

      set({ systemHealth: { ...get().systemHealth, loading: false, error: msg, lastErrorAt: now } })

      return get().systemHealth.data
    }
  },

  async refreshThreadPoolStats() {
    const current = get().threadPoolStats
    if (current.loading) return current.data

    set({ threadPoolStats: { ...current, loading: true, error: null } })

    try {
      const stats = await get().getThreadPoolStats()
      const nextData = ThreadPoolManagerStats.from(stats ?? {})
      const now = Date.now()

      set({
        threadPoolStats: {
          ...get().threadPoolStats,
          data: nextData,
          lastUpdated: now,
          lastSuccessAt: now,
          loading: false,
          error: null,
        },
      })

      return nextData
    } catch (error: unknown) {
      const msg = getErrorMessage(error) || 'Failed to fetch thread pool stats'
      const now = Date.now()

      set({ threadPoolStats: { ...get().threadPoolStats, loading: false, error: msg, lastErrorAt: now } })

      return get().threadPoolStats.data
    }
  },

  async refreshFuseStats() {
    const current = get().fuseStats
    if (current.loading) return current.data

    set({ fuseStats: { ...current, loading: true, error: null } })

    try {
      const stats = await get().getFuseStats()
      const nextData = FuseStats.from(stats ?? {})
      const now = Date.now()

      set({
        fuseStats: {
          ...get().fuseStats,
          data: nextData,
          lastUpdated: now,
          lastSuccessAt: now,
          loading: false,
          error: null,
        },
      })

      return nextData
    } catch (error: unknown) {
      const msg = getErrorMessage(error) || 'Failed to fetch FUSE stats'
      const now = Date.now()

      set({ fuseStats: { ...get().fuseStats, loading: false, error: msg, lastErrorAt: now } })

      return get().fuseStats.data
    }
  },

  async refreshDbStats() {
    const current = get().dbStats
    if (current.loading) return current.data

    set({ dbStats: { ...current, loading: true, error: null } })

    try {
      const stats = await get().getDbStats()
      const nextData = DbStats.from(stats ?? {})
      const now = Date.now()

      set({
        dbStats: {
          ...get().dbStats,
          data: nextData,
          lastUpdated: now,
          lastSuccessAt: now,
          loading: false,
          error: null,
        },
      })

      return nextData
    } catch (error: unknown) {
      const msg = getErrorMessage(error) || 'Failed to fetch database health'
      const now = Date.now()

      set({ dbStats: { ...get().dbStats, loading: false, error: msg, lastErrorAt: now } })

      return get().dbStats.data
    }
  },

  async refreshFsCacheStats() {
    const current = get().fsCacheStats
    if (current.loading) return current.data // dogpile protection

    set({ fsCacheStats: { ...current, loading: true, error: null } })

    try {
      const stats = await get().getFsCacheStats()
      const nextData = new CacheStats(stats ?? {})
      const now = Date.now()

      set({
        fsCacheStats: {
          ...get().fsCacheStats,
          data: nextData,
          lastUpdated: now,
          lastSuccessAt: now,
          loading: false,
          error: null,
        },
      })

      return nextData
    } catch (error: unknown) {
      const msg = getErrorMessage(error) || 'Failed to fetch FS cache stats'
      const now = Date.now()

      set({ fsCacheStats: { ...get().fsCacheStats, loading: false, error: msg, lastErrorAt: now } })

      return get().fsCacheStats.data
    }
  },

  async refreshHttpCacheStats() {
    const current = get().httpCacheStats
    if (current.loading) return current.data

    set({ httpCacheStats: { ...current, loading: true, error: null } })

    try {
      const stats = await get().getHttpCacheStats()
      const nextData = new CacheStats(stats ?? {})
      const now = Date.now()

      set({
        httpCacheStats: {
          ...get().httpCacheStats,
          data: nextData,
          lastUpdated: now,
          lastSuccessAt: now,
          loading: false,
          error: null,
        },
      })

      return nextData
    } catch (error: unknown) {
      const msg = getErrorMessage(error) || 'Failed to fetch HTTP cache stats'
      const now = Date.now()

      set({ httpCacheStats: { ...get().httpCacheStats, loading: false, error: msg, lastErrorAt: now } })

      return get().httpCacheStats.data
    }
  },

  startSystemHealthPolling(intervalMs = 7500) {
    if (get()._systemHealthPoller) return

    void get().refreshSystemHealth()

    const id = window.setInterval(() => {
      void get().refreshSystemHealth()
    }, intervalMs)

    set({ _systemHealthPoller: id })
  },

  stopSystemHealthPolling() {
    const id = get()._systemHealthPoller
    if (id) {
      window.clearInterval(id)
      set({ _systemHealthPoller: null })
    }
  },

  startThreadPoolStatsPolling(intervalMs = 7500) {
    if (get()._threadPoolStatsPoller) return

    void get().refreshThreadPoolStats()

    const id = window.setInterval(() => {
      void get().refreshThreadPoolStats()
    }, intervalMs)

    set({ _threadPoolStatsPoller: id })
  },

  stopThreadPoolStatsPolling() {
    const id = get()._threadPoolStatsPoller
    if (id) {
      window.clearInterval(id)
      set({ _threadPoolStatsPoller: null })
    }
  },

  startFuseStatsPolling(intervalMs = 7500) {
    if (get()._fuseStatsPoller) return

    void get().refreshFuseStats()

    const id = window.setInterval(() => {
      void get().refreshFuseStats()
    }, intervalMs)

    set({ _fuseStatsPoller: id })
  },

  stopFuseStatsPolling() {
    const id = get()._fuseStatsPoller
    if (id) {
      window.clearInterval(id)
      set({ _fuseStatsPoller: null })
    }
  },

  startDbStatsPolling(intervalMs = 7500) {
    if (get()._dbStatsPoller) return

    void get().refreshDbStats()

    const id = window.setInterval(() => {
      void get().refreshDbStats()
    }, intervalMs)

    set({ _dbStatsPoller: id })
  },

  stopDbStatsPolling() {
    const id = get()._dbStatsPoller
    if (id) {
      window.clearInterval(id)
      set({ _dbStatsPoller: null })
    }
  },

  startFsCacheStatsPolling(intervalMs = 7500) {
    if (get()._fsCachePoller) return

    void get().refreshFsCacheStats()

    const id = window.setInterval(() => {
      void get().refreshFsCacheStats()
    }, intervalMs)

    set({ _fsCachePoller: id })
  },

  stopFsCacheStatsPolling() {
    const id = get()._fsCachePoller
    if (id) {
      window.clearInterval(id)
      set({ _fsCachePoller: null })
    }
  },

  startHttpCacheStatsPolling(intervalMs = 7500) {
    if (get()._httpCachePoller) return

    void get().refreshHttpCacheStats()

    const id = window.setInterval(() => {
      void get().refreshHttpCacheStats()
    }, intervalMs)

    set({ _httpCachePoller: id })
  },

  stopHttpCacheStatsPolling() {
    const id = get()._httpCachePoller
    if (id) {
      window.clearInterval(id)
      set({ _httpCachePoller: null })
    }
  },
}))
