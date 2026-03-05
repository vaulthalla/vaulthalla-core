import React from 'react'

export const AdminGrid = ({ children }: { children: React.ReactNode }) => (
  <div className="mb-6 grid grid-cols-1 gap-4 lg:grid-cols-2 xl:grid-cols-3">{children}</div>
)
