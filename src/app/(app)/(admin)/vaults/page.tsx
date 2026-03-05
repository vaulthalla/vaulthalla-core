import VaultsClientPage from './page.client'
import { AdminPage } from '@/components/admin/AdminPage'
import Link from 'next/link'
import { Button } from '@/components/Button'
import Plus from '@/fa-regular/plus.svg'
import { AdminGrid } from '@/components/admin/AdminGrid'

const VaultsPage = () => {
  const title = 'Vaults'
  const description = 'Choose a vault to manage.'
  const props = { title, description }

  const Grid = () => (
    <AdminGrid>
      <VaultsClientPage />
    </AdminGrid>
  )

  const AddButton = () => (
    <Link href="/src/app/(app)/(admin)/vaults)/vaults/add">
      <Button type="button">
        <Plus className="text-secondary mr-2 fill-current" /> Add Vault
      </Button>
    </Link>
  )

  return (
    <AdminPage {...props}>
      <AddButton />
      <Grid />
    </AdminPage>
  )
}

export default VaultsPage
