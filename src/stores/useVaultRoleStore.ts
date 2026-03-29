import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import { useWebSocketStore } from '@/stores/useWebSocket'
import { VaultRole } from '@/models/role'
import { WSCommandPayload } from '@/util/webSocketCommands'

interface VaultRoleStore {
  vaultRoles: VaultRole[]
  fetchVaultRoles: () => Promise<void>
  addVaultRole: (payload: WSCommandPayload<'role.vault.add'>) => Promise<void>
  removeVaultRole: (payload: WSCommandPayload<'role.vault.delete'>) => Promise<void>
  updateVaultRole: (payload: WSCommandPayload<'role.vault.update'>) => Promise<void>
  getVaultRole: (payload: WSCommandPayload<'role.vault.get'>) => Promise<VaultRole | null | undefined>
  getVaultRoleByName: (payload: WSCommandPayload<'role.vault.get.byName'>) => Promise<VaultRole | null | undefined>
}

export const useVaultRoleStore = create<VaultRoleStore>()(
  persist(
    (set, get) => ({
      vaultRoles: [],

      async fetchVaultRoles() {
        const ws = useWebSocketStore.getState()
        await ws.waitForConnection()
        const response = await ws.sendCommand('roles.vault.list', null)
        set({ vaultRoles: response.roles.map(r => VaultRole.fromData(r)) })
      },

      async addVaultRole(payload) {
        const ws = useWebSocketStore.getState()
        await ws.waitForConnection()
        await ws.sendCommand('role.vault.add', payload)
        await get().fetchVaultRoles()
      },

      async removeVaultRole({ id }) {
        const ws = useWebSocketStore.getState()
        await ws.waitForConnection()
        await ws.sendCommand('role.vault.delete', { id })

        set(state => ({ vaultRoles: state.vaultRoles.filter(r => r.id !== id) }))
      },

      async updateVaultRole(payload) {
        const ws = useWebSocketStore.getState()
        await ws.waitForConnection()
        await ws.sendCommand('role.vault.update', payload)
        await get().fetchVaultRoles()
      },

      async getVaultRole({ id }) {
        const cached = get().vaultRoles.find(r => r.id === id)
        if (cached) return cached

        const ws = useWebSocketStore.getState()
        await ws.waitForConnection()
        const response = await ws.sendCommand('role.vault.get', { id })
        return VaultRole.fromData(response.vault)
      },

      async getVaultRoleByName({ name }) {
        const cached = get().vaultRoles.find(r => r.name === name)
        if (cached) return cached

        const ws = useWebSocketStore.getState()
        await ws.waitForConnection()
        const response = await ws.sendCommand('role.vault.get.byName', { name })
        return VaultRole.fromData(response.vault)
      },
    }),
    {
      name: 'vaulthalla-vault-roles',
      partialize: state => ({ vaultRoles: state.vaultRoles }),
      onRehydrateStorage: state => {
        console.log('[VaultRoleStore] Rehydrating...')
        return async () => {
          try {
            await useWebSocketStore.getState().waitForConnection()
            await state?.fetchVaultRoles?.()
            console.log('[VaultRoleStore] Vault roles fetch complete')
          } catch (err) {
            console.error('[VaultRoleStore] Rehydrate fetch failed:', err)
          }
        }
      },
    },
  ),
)
