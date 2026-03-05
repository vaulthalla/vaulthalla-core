import type { Metadata } from 'next'
import Sidebar from '@/components/sidebar/Sidebar.server'
import RequireAuth from '@/components/auth/RequireAuth'

export const metadata: Metadata = { title: 'Vaulthalla | Files', description: 'The Final Cloud' }

export default function FilesLayout({ children }: { children: React.ReactNode }) {
  return children
}
