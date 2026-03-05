'use client'

import { useAuthStore } from '@/stores/authStore'
import { Button } from '@/components/Button'
import { useRouter } from 'next/navigation'
import { useSidebar } from '@/hooks/useSidebar'

export const LogoutButton = () => {
  const router = useRouter()
  const { isCollapsed } = useSidebar()

  return (
    <Button
      variant="destructive"
      onClick={() => {
        useAuthStore.getState().logout()
        router.push('/login')
      }}>
      {isCollapsed ? '⏻' : 'Logout'}
    </Button>
  )
}
