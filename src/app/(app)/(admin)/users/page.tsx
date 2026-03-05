import UsersClientPage from '@/app/(app)/(admin)/users/page.client'
import { AdminGrid } from '@/components/admin/AdminGrid'
import { AdminPage } from '@/components/admin/AdminPage'

const UsersPage = () => {
  const title = 'Users'
  const description = 'Manage users and their permissions in your vault.'
  const props = { title, description }

  return (
    <AdminPage {...props}>
      <AdminGrid>
        <UsersClientPage />
      </AdminGrid>
    </AdminPage>
  )
}

export default UsersPage
