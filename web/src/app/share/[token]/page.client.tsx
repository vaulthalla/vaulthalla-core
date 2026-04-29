'use client'

import React, { FormEvent, useEffect, useState } from 'react'
import { Filesystem } from '@/components/fs/Filesystem'
import { FileDropOverlay } from '@/components/fs/FileDropOverlay'
import UploadProgress from '@/components/loading/UploadProgress'
import { useFSStore } from '@/stores/fsStore'
import { useVaultShareStore } from '@/stores/vaultShareStore'

const SharePageClient = ({ token }: { token: string }) => {
  const [email, setEmail] = useState('')
  const [code, setCode] = useState('')
  const { status, share, error, openSession, startEmailChallenge, confirmEmailChallenge, clearShareSession } = useVaultShareStore()
  const { files, path, setPath, fetchFiles, enterShareMode, exitShareMode } = useFSStore()

  useEffect(() => {
    enterShareMode()
    openSession(token).catch(() => undefined)

    return () => {
      clearShareSession()
      exitShareMode()
    }
  }, [clearShareSession, enterShareMode, exitShareMode, openSession, token])

  useEffect(() => {
    if (status === 'ready') fetchFiles().catch(() => undefined)
  }, [fetchFiles, status])

  const submitEmail = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    startEmailChallenge(email).catch(() => undefined)
  }

  const submitCode = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    confirmEmailChallenge(code).catch(() => undefined)
  }

  const title = share?.public_label || share?.metadata?.label?.toString() || 'Shared files'

  return (
    <main className="min-h-screen bg-gray-950 px-4 py-6 text-white sm:px-8">
      <div className="mx-auto flex max-w-6xl flex-col gap-4">
        <header className="flex flex-col gap-2 border-b border-white/10 pb-4">
          <p className="text-sm text-cyan-300">Vaulthalla Share</p>
          <h1 className="text-2xl font-semibold">{title}</h1>
          {status === 'ready' && (
            <div className="flex items-center gap-2 text-sm text-gray-300">
              <button className="text-cyan-300 hover:underline" onClick={() => setPath('/')}>
                /
              </button>
              {path
                .split('/')
                .filter(Boolean)
                .map((part, index, parts) => {
                  const fullPath = `/${parts.slice(0, index + 1).join('/')}`
                  return (
                    <React.Fragment key={fullPath}>
                      <span>/</span>
                      <button className="text-cyan-300 hover:underline" onClick={() => setPath(fullPath)}>
                        {part}
                      </button>
                    </React.Fragment>
                  )
                })}
            </div>
          )}
        </header>

        {status === 'opening' || status === 'idle' ? <div className="text-gray-300">Opening share...</div> : null}

        {status === 'email_required' && (
          <section className="max-w-md rounded border border-white/10 bg-gray-900 p-4">
            <form className="flex flex-col gap-3" onSubmit={submitEmail}>
              <label className="flex flex-col gap-1 text-sm">
                Email
                <input
                  className="rounded border border-gray-700 bg-gray-950 px-3 py-2 text-white"
                  type="email"
                  value={email}
                  onChange={event => setEmail(event.target.value)}
                  required
                />
              </label>
              <button className="rounded bg-cyan-500 px-3 py-2 font-medium text-gray-950" type="submit">
                Send Code
              </button>
            </form>

            <form className="mt-4 flex flex-col gap-3" onSubmit={submitCode}>
              <label className="flex flex-col gap-1 text-sm">
                Code
                <input
                  className="rounded border border-gray-700 bg-gray-950 px-3 py-2 text-white"
                  value={code}
                  onChange={event => setCode(event.target.value)}
                  required
                />
              </label>
              <button className="rounded bg-white px-3 py-2 font-medium text-gray-950" type="submit">
                Verify
              </button>
            </form>
          </section>
        )}

        {status === 'ready' && (
          <FileDropOverlay>
            <UploadProgress />
            <Filesystem files={files} />
          </FileDropOverlay>
        )}

        {status === 'error' && (
          <div className="rounded border border-red-500/40 bg-red-950/40 p-4 text-red-100">
            {error || 'This share could not be opened.'}
          </div>
        )}
      </div>
    </main>
  )
}

export default SharePageClient
