'use client'

import { usePermsStore } from '@/stores/permissionStore'
import RolesComponent from '@/components/roles/RolesComponent'

const VaultRolesClientPage = () => {
  const { vaultRoles } = usePermsStore()
  return <RolesComponent roles={vaultRoles} />
}

export default VaultRolesClientPage
