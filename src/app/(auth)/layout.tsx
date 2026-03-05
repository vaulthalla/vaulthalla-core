import type { Metadata } from 'next'
import { Geist, Geist_Mono } from 'next/font/google'
import '@/app/globals.css'
import { WebSocketProvider } from '@/components/WebSocketProvider'

const geistSans = Geist({ variable: '--font-geist-sans', subsets: ['latin'] })

const geistMono = Geist_Mono({ variable: '--font-geist-mono', subsets: ['latin'] })

export const metadata: Metadata = { title: 'Login | Vaulthalla', description: 'The Final Cloud' }

const MainLayout = ({ children }: { children: React.ReactNode }) => (
  <div className="flex h-screen w-full text-white">
    <main className="my-4 flex flex-1 justify-center overflow-y-auto">{children}</main>
  </div>
)

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body className={`${geistSans.variable} ${geistMono.variable} bg-background antialiased`}>
        <WebSocketProvider />
        <MainLayout>{children}</MainLayout>
      </body>
    </html>
  )
}
