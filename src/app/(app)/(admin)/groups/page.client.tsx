'use client'

import { useGroupStore } from '@/stores/groupStore'
import GroupCard from '@/components/groups/GroupCard'

const GroupsClientPage = () => {
  const { groups } = useGroupStore()

  return groups.length > 0 ?
      <div className="3xl:grid-cols-3 grid gap-6 sm:grid-cols-1 md:grid-cols-2">
        {groups.map(group => (
          <GroupCard {...group} key={group.id} />
        ))}
      </div>
    : <p className="text-center text-white/60">No groups found.</p>
}

export default GroupsClientPage
