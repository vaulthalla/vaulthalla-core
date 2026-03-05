import EditRoleClientPage from '@/app/(app)/(admin)/roles/[role_id]/page.client'

const EditRolePage = async ({ params }: { params: Promise<{ role_id: string }> }) => {
  const { role_id } = await params
  const id = Number(role_id)

  return <EditRoleClientPage id={id} />
}

export default EditRolePage
