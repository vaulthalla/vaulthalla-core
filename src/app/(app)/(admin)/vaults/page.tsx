import VaultsClientPage from './page.client'

const VaultsPage = () => {
  return (
    <div className="p-6 md:p-10">
      <div className="mb-6">
        <h1 className="text-3xl font-semibold text-cyan-100">Vaults</h1>
        <p className="mt-2 text-cyan-300/80">Choose a vault to manage.</p>
      </div>

      <VaultsClientPage />
    </div>
  )
}

export default VaultsPage
