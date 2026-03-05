import FilesClientPage from '@/app/(app)/(fs)/fs/page.client'
import VaultBreadcrumbs from '@/components/files/VaultBreadcrumbs'
import CopiedItemIndicator from '@/components/files/CopiedItemIndicator'

const FSPage = () => {
  return (
    <div className="mx-8 h-full w-full">
      <CopiedItemIndicator />
      <VaultBreadcrumbs className="mb-3" />
      <FilesClientPage />
    </div>
  )
}

export default FSPage
