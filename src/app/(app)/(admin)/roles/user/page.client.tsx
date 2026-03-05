'use client'

import { usePermsStore } from '@/stores/permissionStore'
import RolesComponent from '@/components/roles/RolesComponent'

const UserRolesClientPage = () => {
  const { userRoles } = usePermsStore()
  return <RolesComponent roles={userRoles} />
}

export default UserRolesClientPage
