import type { ComponentType, SVGProps } from 'react'

export type NavIcon = ComponentType<SVGProps<SVGSVGElement>>

export type NavItem = { href: string; label: string; icon: NavIcon; exact?: boolean; subItems?: NavItem[] }

export type NavConfig = { items: NavItem[] }
