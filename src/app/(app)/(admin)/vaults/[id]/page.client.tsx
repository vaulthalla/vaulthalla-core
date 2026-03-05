'use client'

import { useVaultStore } from '@/stores/vaultStore'
import { useEffect, useState } from 'react'
import { LocalDiskVault, S3Vault, Vault } from '@/models/vaults'
import CircleNotchLoader from '@/components/loading/CircleNotchLoader'
import VaultStatsDashboard from '@/components/vault/VaultStatsDashboard/Component'
import VaultHero from '@/components/vault/VaultHero'

const VaultPage = ({ id }: { id: number }) => {
  const [vault, setVault] = useState<LocalDiskVault | S3Vault | Vault | null>(null)

  useEffect(() => {
    const fetchVault = async () => {
      const vault = await useVaultStore.getState().getVault({ id })
      if (vault) setVault(vault)
    }

    fetchVault()
  }, [id])

  if (!vault) return <CircleNotchLoader />

  return (
    <div className="mb-4 text-center">
      <VaultHero vault={vault} />
      <VaultStatsDashboard vault_id={id} />
    </div>
  )
}

export default VaultPage
