import Logo from '@/components/Logo'
import { NavList } from '@/components/nav/NavList'
import type { NavConfig } from '@/components/nav/types'

export const FSRail = ({ config }: { config: NavConfig }) => {
  return (
    <aside className="hidden h-full w-20 border-r border-white/20 bg-linear-to-b from-white/10 to-black/20 backdrop-blur-xl backdrop-saturate-150 md:block">
      <div className="flex h-full flex-col items-center space-y-3 p-2">
        <div className="pt-1">
          <Logo />
        </div>

        <div className="grow" />

        <nav className="bg-secondary text-primary w-[85%] rounded-xl p-2 shadow-lg">
          <NavList items={config.items} compact />
        </nav>

        <div className="grow" />
      </div>
    </aside>
  )
}
