import DashboardOverviewComponent from '@/components/dashboard/DashboardOverview'

const DashboardPage = () => {
  return (
    <div className="h-full min-h-screen w-full px-6 py-8">
      <div className="mx-auto w-full max-w-6xl">
        <DashboardOverviewComponent />
      </div>
    </div>
  )
}

export default DashboardPage
