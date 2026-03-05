import { RoleCategoryCard } from './RoleCategoryCard'
import UserIcon from '@/fa-duotone/user.svg'
import VaultIcon from '@/fa-duotone/vault.svg'

export const RoleCategoryGrid = () => {
  return (
    <div className="grid gap-4 sm:grid-cols-2">
      <RoleCategoryCard
        title="User Roles"
        description="Permissions and roles assigned to users."
        href="/roles/user"
        Icon={UserIcon}
      />

      <RoleCategoryCard
        title="Vault Roles"
        description="Roles that apply within a vault context."
        href="/roles/vault"
        Icon={VaultIcon}
      />
    </div>
  )
}
