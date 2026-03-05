import React from 'react'

interface AdminPageBaseProps {
  title: string
  description: string
  children: React.ReactNode
}

export const AdminPage = ({ title, description, children }: AdminPageBaseProps) => {
  return (
    <div className="p-6 md:p-10">
      <div className="mb-6">
        <h1 className="text-3xl font-semibold text-cyan-100">{title}</h1>
        <p className="mt-2 text-cyan-300/80">{description}</p>
      </div>
      {children}
    </div>
  )
}
