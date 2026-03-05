import UserFullCard from '@/components/users/UserFullCard'

const UserPage = async ({ params }: { params: Promise<{ name: string }> }) => {
  const { name } = await params

  return <UserFullCard name={name} />
}

export default UserPage
