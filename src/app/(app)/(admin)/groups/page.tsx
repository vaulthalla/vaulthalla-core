import GroupsClientPage from '@/app/(app)/(admin)/groups/page.client'

const GroupsPage = () => {
  return (
    <div className="flex h-full flex-col items-center justify-center">
      <h1 className="mb-4 text-2xl font-bold">Manage Groups</h1>
      <GroupsClientPage />
    </div>
  )
}

export default GroupsPage
