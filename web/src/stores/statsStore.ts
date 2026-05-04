import { create } from 'zustand'
import { WSCommandPayload } from '@/util/webSocketCommands'
import { useWebSocketStore } from '@/stores/useWebSocket'
import { VaultStats } from '@/models/stats/vaultStats'
import { CacheStats } from '@/models/stats/cacheStats'
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

interface StatsStore {
  // Two independent caches, same type
  fsCacheStats: CacheStatsWrapper
  httpCacheStats: CacheStatsWrapper
  systemHealth: SystemHealthWrapper
  threadPoolStats: ThreadPoolStatsWrapper

  // Fetchers (raw RPC)
  getVaultStats: (payload: WSCommandPayload<'stats.vault'>) => Promise<VaultStats>
  getSystemHealth: (payload?: WSCommandPayload<'stats.system.health'>) => Promise<SystemHealth>
  getThreadPoolStats: (payload?: WSCommandPayload<'stats.system.threadpools'>) => Promise<ThreadPoolManagerStats>
  getFsCacheStats: (payload?: WSCommandPayload<'stats.fs.cache'>) => Promise<CacheStats>
  getHttpCacheStats: (payload?: WSCommandPayload<'stats.http.cache'>) => Promise<CacheStats>

  // Refresh helpers
  refreshSystemHealth: () => Promise<SystemHealth>
  refreshThreadPoolStats: () => Promise<ThreadPoolManagerStats>
  refreshFsCacheStats: () => Promise<CacheStats>
  refreshHttpCacheStats: () => Promise<CacheStats>

  // Polling (separate pollers)
  startSystemHealthPolling: (intervalMs?: number) => void
  stopSystemHealthPolling: () => void
  startThreadPoolStatsPolling: (intervalMs?: number) => void
  stopThreadPoolStatsPolling: () => void
  startFsCacheStatsPolling: (intervalMs?: number) => void
  stopFsCacheStatsPolling: () => void
  startHttpCacheStatsPolling: (intervalMs?: number) => void
  stopHttpCacheStatsPolling: () => void

  // internal
  _systemHealthPoller: number | null
  _threadPoolStatsPoller: number | null
  _fsCachePoller: number | null
  _httpCachePoller: number | null
}

export const useStatsStore = create<StatsStore>((set, get) => ({
  fsCacheStats: makeCacheWrapper(),
  httpCacheStats: makeCacheWrapper(),
  systemHealth: makeSystemHealthWrapper(),
  threadPoolStats: makeThreadPoolStatsWrapper(),

  _systemHealthPoller: null,
  _threadPoolStatsPoller: null,
  _fsCachePoller: null,
  _httpCachePoller: null,

  async getVaultStats(vault_id) {
    const ws = useWebSocketStore.getState()
    await ws.waitForConnection()
    const response = await ws.sendCommand('stats.vault', vault_id)
    return response.stats
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
