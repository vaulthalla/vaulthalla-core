import { AdminPage } from '@/components/admin/AdminPage'
import APIKeysClientPage from '@/app/(app)/(admin)/api-keys/page.client'
import { AddButton } from '@/components/admin/AddButton'

const APIKeysPage = () => {
  const title = 'API Keys'
  const description = 'Manage your API keys for cloud mounts here. You can create, edit, and delete keys as needed.'
  const props = { title, description }

  return (
    <AdminPage {...props}>
      <AddButton title="Add API Key" href="/api-keys/add" />
      <APIKeysClientPage />
    </AdminPage>
  )
}

export default APIKeysPage
