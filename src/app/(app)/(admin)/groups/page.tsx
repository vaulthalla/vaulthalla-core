import GroupsClientPage from '@/app/(app)/(admin)/groups/page.client'
import { AdminPage } from '@/components/admin/AdminPage'

const GroupsPage = () => {
  const title = 'Manage Groups'
  const description = 'Create and manage groups to organize users and permissions effectively.'
  const props = { title, description }

  return (
    <AdminPage {...props}>
      <GroupsClientPage />
    </AdminPage>
  )
}

export default GroupsPage
