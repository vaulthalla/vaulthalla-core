import { Button } from '@/components/Button'
import Link from 'next/link'
import { AdminPage } from '@/components/admin/AdminPage'
import APIKeysClientPage from '@/app/(app)/(admin)/api-keys/page.client'

const APIKeysPage = () => {
  const title = 'API Keys'
  const description = 'Manage your API keys for cloud mounts here. You can create, edit, and delete keys as needed.'
  const props = { title, description }

  const AddButton = () => (
    <Link href="/src/app/(app)/(admin)/api-keys/add">
      <Button className="w-full cursor-pointer rounded-lg bg-blue-600 px-4 font-semibold text-white transition-colors duration-200 hover:bg-blue-700">
        + Create New API Key
      </Button>
    </Link>
  )

  return (
    <AdminPage {...props}>
      <AddButton />
      <APIKeysClientPage />
    </AdminPage>
  )
}

export default APIKeysPage
