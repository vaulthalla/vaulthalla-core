import VaultsClientPage from './page.client'
import { AdminPage } from '@/components/admin/AdminPage'
import { AdminGrid } from '@/components/admin/AdminGrid'
import { AddButton } from '@/components/admin/AddButton'

const VaultsPage = () => {
  const title = 'Vaults'
  const description = 'Choose a vault to manage.'
  const props = { title, description }

  const Grid = () => (
    <AdminGrid>
      <VaultsClientPage />
    </AdminGrid>
  )

  return (
    <AdminPage {...props}>
      <AddButton title="Add Vault" href="/vaults/add" />
      <Grid />
    </AdminPage>
  )
}

export default VaultsPage
