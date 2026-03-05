'use client'

import { useApiKeyStore } from '@/stores/apiKeyStore'
import { Button } from '@/components/Button'
import Link from 'next/link'
import APIKeyCard from '@/components/api_keys/APIKeyCard'

const APIKeysComponent = () => {
  const { apiKeys } = useApiKeyStore()

  return (
    <div>
      <h1 className="mb-4 text-4xl font-semibold">API Keys</h1>
      <p className="text-gray-200">
        Manage your API keys for cloud mounts here. You can create, edit, and delete keys as needed.
      </p>
      {apiKeys && apiKeys.length > 0 ?
        <ul className="my-6">
          {apiKeys.map(apiKey => (
            <li key={apiKey.api_key_id} className="my-4">
              <Link href="/src/app/(app)/(admin)/api-keys/[slug]" as={`/dashboard/api-keys/${apiKey.api_key_id}`}>
                <APIKeyCard {...apiKey} />
              </Link>
            </li>
          ))}
        </ul>
      : <p className="text-gray-500">No API keys found. Create one to get started.</p>}

      <Link href="/src/app/(app)/(admin)/api-keys/add">
        <Button className="w-full cursor-pointer rounded-lg bg-blue-600 px-4 font-semibold text-white transition-colors duration-200 hover:bg-blue-700">
          + Create New API Key
        </Button>
      </Link>
    </div>
  )
}

export default APIKeysComponent
