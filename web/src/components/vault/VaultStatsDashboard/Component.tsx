import { useStatsStore } from '@/stores/statsStore'
import CapacityStats from '@/components/vault/VaultStatsDashboard/CapacityStats/Component'
import { useEffect, useState } from 'react'
import { VaultStats } from '@/models/stats/vaultStats'
import SyncHealth from '@/components/vault/VaultStatsDashboard/SyncHealth/Component'
import VaultActivity from '@/components/vault/VaultStatsDashboard/VaultActivity/Component'
import ShareStats from '@/components/vault/VaultStatsDashboard/ShareStats/Component'

const VaultStatsDashboard = ({ vault_id }: { vault_id: number }) => {
  const [stats, setStats] = useState<VaultStats | undefined>(undefined)

  useEffect(() => {
    const fetchStats = async () => setStats(await useStatsStore.getState().getVaultStats({ vault_id }))
    fetchStats()
  }, [vault_id])

  return (
    stats && (
      <div className="VaultStatsDashboard">
        <CapacityStats capacityStats={stats.capacity} />
        <SyncHealth vaultId={vault_id} initialLatestEvent={stats.latest_sync_event} />
        <VaultActivity vaultId={vault_id} />
        <ShareStats vaultId={vault_id} />
      </div>
    )
  )
}

export default VaultStatsDashboard
