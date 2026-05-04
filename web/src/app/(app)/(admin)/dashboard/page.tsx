import CacheStatsComponent from '@/components/stats/CacheStats'
import SystemHealthComponent from '@/components/stats/SystemHealth'
import ThreadPoolStatsComponent from '@/components/stats/ThreadPoolStats'

const DashboardPage = () => {
  return (
    <div className="h-full min-h-screen w-full px-6 py-8">
      <div className="mx-auto flex w-full max-w-6xl flex-col gap-6">
        <SystemHealthComponent />
        <ThreadPoolStatsComponent />
        <CacheStatsComponent source="fs" />
        <CacheStatsComponent source="http" />
      </div>
    </div>
  )
}

export default DashboardPage
