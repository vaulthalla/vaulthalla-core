'use client'

import React, { FormEvent, useEffect, useMemo, useState } from 'react'
import { Filesystem } from '@/components/fs/Filesystem'
import { FileDropOverlay } from '@/components/fs/FileDropOverlay'
import UploadProgress from '@/components/loading/UploadProgress'
import { useFSStore } from '@/stores/fsStore'
import { useVaultShareStore } from '@/stores/vaultShareStore'
import { formatShareDate, hasShareOperation, shareOperationLabel } from '@/util/shareOperations'

const parentPath = (path: string) => {
  const parts = path.split('/').filter(Boolean)
  parts.pop()
  return parts.length ? `/${parts.join('/')}` : '/'
}

const Alert = ({ tone = 'info', children }: { tone?: 'info' | 'error' | 'success'; children: React.ReactNode }) => {
  const styles =
    tone === 'error' ? 'border-red-500/40 bg-red-950/40 text-red-100'
    : tone === 'success' ? 'border-emerald-500/40 bg-emerald-950/30 text-emerald-100'
    : 'border-cyan-500/30 bg-cyan-950/30 text-cyan-100'

  return <div className={`rounded border p-3 text-sm ${styles}`}>{children}</div>
}

const SharePageClient = ({ token }: { token: string }) => {
  const [email, setEmail] = useState('')
  const [code, setCode] = useState('')
  const {
    status,
    share,
    error,
    openSession,
    startEmailChallenge,
    confirmEmailChallenge,
    clearShareSession,
    challengeId,
    emailSubmitting,
    codeSubmitting,
    challengeStartedAt,
  } = useVaultShareStore()
  const {
    files,
    path,
    setPath,
    fetchFiles,
    enterShareMode,
    exitShareMode,
    uploadError,
    uploadLabel,
    uploading,
    uploadProgress,
    previewing,
    previewError,
    sharePreview,
    downloading,
    downloadLabel,
    downloadProgress,
    downloadError,
    downloadFile,
    previewFile,
  } = useFSStore()

  const canList = hasShareOperation(share?.allowed_ops, 'list')
  const canPreview = hasShareOperation(share?.allowed_ops, 'preview')
  const canDownload = hasShareOperation(share?.allowed_ops, 'download')
  const canUpload = hasShareOperation(share?.allowed_ops, 'upload')
  const isFileShare = share?.target_type === 'file'
  const isDirectoryShare = share?.target_type === 'directory'
  const title = share?.public_label || share?.metadata?.label?.toString() || 'Shared files'
  const expiresAt = share?.expires_at ? formatShareDate(share.expires_at) : null
  const pathParts = useMemo(() => path.split('/').filter(Boolean), [path])
  const previewDataUrl = sharePreview ? `data:${sharePreview.mime_type};base64,${sharePreview.data_base64}` : null
  const previewIsImage = Boolean(sharePreview?.mime_type.startsWith('image/'))
  const previewIsPdf = sharePreview?.mime_type === 'application/pdf'

  useEffect(() => {
    enterShareMode()
    openSession(token).catch(() => undefined)

    return () => {
      clearShareSession()
      exitShareMode()
    }
  }, [clearShareSession, enterShareMode, exitShareMode, openSession, token])

  useEffect(() => {
    if (status === 'ready' && canList) fetchFiles().catch(() => undefined)
  }, [canList, fetchFiles, status])

  useEffect(() => {
    if (status !== 'ready' || !isFileShare || !canPreview || previewing || previewError || sharePreview) return
    previewFile('/').catch(() => undefined)
  }, [canPreview, isFileShare, previewError, previewFile, previewing, sharePreview, status])

  const submitEmail = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    await startEmailChallenge(email).catch(() => undefined)
  }

  const submitCode = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    await confirmEmailChallenge(code).catch(() => undefined)
  }

  const downloadRootFile = () => {
    if (!canDownload || downloading) return
    downloadFile('/').catch(() => undefined)
  }

  return (
    <main className="min-h-screen bg-gray-950 px-3 py-4 text-white sm:px-6">
      <div className="mx-auto flex max-w-7xl flex-col gap-4">
        <header className="rounded-lg border border-white/10 bg-gray-900/90 p-3 shadow-lg">
          <div className="flex flex-col gap-3 md:flex-row md:items-center md:justify-between">
            <div className="min-w-0">
              <p className="text-xs font-medium uppercase tracking-wide text-cyan-300">Vaulthalla Share</p>
              <h1 className="truncate text-xl font-semibold">{title}</h1>
              <div className="mt-1 flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-gray-400">
                {share?.root_path && <span className="truncate">{share.root_path}</span>}
                {expiresAt && <span>Expires {expiresAt}</span>}
              </div>
            </div>

            {share && (
              <div className="flex flex-wrap items-center gap-2 text-xs text-gray-300 md:justify-end">
                {canList && <span className="rounded border border-white/10 bg-white/5 px-2 py-1">List</span>}
                {canPreview && <span className="rounded border border-white/10 bg-white/5 px-2 py-1">Preview</span>}
                {canDownload && <span className="rounded border border-white/10 bg-white/5 px-2 py-1">Download</span>}
                {canUpload && <span className="rounded border border-white/10 bg-white/5 px-2 py-1">Upload</span>}
                {share.access_mode === 'email_validated' && (
                  <span className="rounded border border-cyan-500/30 bg-cyan-950/40 px-2 py-1 text-cyan-100">Email verified</span>
                )}
              </div>
            )}
          </div>

          {status === 'ready' && (
            <div className="mt-3 flex flex-wrap items-center gap-2 border-t border-white/10 pt-3 text-sm text-gray-300">
              <button className="rounded border border-gray-700 px-2 py-1 text-cyan-300 hover:bg-white/10" onClick={() => setPath('/')}>
                Root
              </button>
              {pathParts.map((part, index, parts) => {
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
              {path !== '/' && (
                <button className="ml-auto rounded border border-gray-700 px-2 py-1 text-gray-200 hover:bg-white/10" onClick={() => setPath(parentPath(path))}>
                  Up
                </button>
              )}
            </div>
          )}
        </header>

        {status === 'opening' || status === 'idle' ? (
          <section className="rounded border border-white/10 bg-gray-900 p-6">
            <div className="h-2 w-full overflow-hidden rounded bg-gray-800">
              <div className="h-full w-1/2 animate-pulse rounded bg-cyan-400" />
            </div>
            <p className="mt-4 text-gray-300">Opening secure share session...</p>
          </section>
        ) : null}

        {status === 'email_required' && (
          <section className="grid gap-4 rounded border border-white/10 bg-gray-900 p-4 md:grid-cols-[minmax(0,0.95fr)_minmax(0,1.05fr)]">
            <div>
              <h2 className="text-xl font-semibold">Email verification required</h2>
              <p className="mt-2 text-sm text-gray-300">
                Enter the email address this share was sent to, then enter the verification code.
              </p>
              {challengeStartedAt && (
                <Alert tone="success">
                  Code requested. Check your email, then enter the code here. You can resend if it expires.
                </Alert>
              )}
              {error && <Alert tone="error">{error}</Alert>}
            </div>

            <div className="space-y-4">
              <form className="flex flex-col gap-3" onSubmit={submitEmail}>
                <label className="flex flex-col gap-1 text-sm">
                  Email
                  <input
                    className="rounded border border-gray-700 bg-gray-950 px-3 py-2 text-white"
                    type="email"
                    value={email}
                    onChange={event => setEmail(event.target.value)}
                    required
                    disabled={emailSubmitting}
                  />
                </label>
                <button className="rounded bg-cyan-500 px-3 py-2 font-medium text-gray-950 disabled:opacity-60" type="submit" disabled={emailSubmitting}>
                  {challengeId ? 'Resend Code' : 'Send Code'}
                </button>
              </form>

              <form className="flex flex-col gap-3" onSubmit={submitCode}>
                <label className="flex flex-col gap-1 text-sm">
                  Code
                  <input
                    className="rounded border border-gray-700 bg-gray-950 px-3 py-2 text-white"
                    value={code}
                    onChange={event => setCode(event.target.value)}
                    required
                    disabled={!challengeId || codeSubmitting}
                  />
                </label>
                <button
                  className="rounded bg-white px-3 py-2 font-medium text-gray-950 disabled:opacity-60"
                  type="submit"
                  disabled={!challengeId || codeSubmitting}>
                  Verify
                </button>
              </form>
            </div>
          </section>
        )}

        {status === 'ready' && (
          <>
            <section className="grid gap-2 md:grid-cols-3">
              <div className="rounded border border-white/10 bg-gray-900/80 p-3">
                <div className="text-xs text-gray-500">Access</div>
                <div className="mt-1 text-sm text-gray-200">{shareOperationLabel(share?.allowed_ops)}</div>
              </div>
              <div className="rounded border border-white/10 bg-gray-900/80 p-3">
                <div className="text-xs text-gray-500">Files</div>
                <div className="mt-1 text-sm text-gray-200">
                  {canPreview ? 'Click a file to preview it.'
                  : canDownload ? 'Click a file to download it.'
                  : 'Not allowed for this share.'}
                </div>
              </div>
              <div className="rounded border border-white/10 bg-gray-900/80 p-3">
                <div className="text-xs text-gray-500">Upload</div>
                <div className="mt-1 text-sm text-gray-200">
                  {canUpload && isDirectoryShare ? 'Drop files into this page to upload.' : 'Not allowed for this share.'}
                </div>
              </div>
            </section>

            {(downloading || downloadError) && (
              <Alert tone={downloadError ? 'error' : 'info'}>
                {downloadError || `Downloading ${downloadLabel || 'file'}... ${Math.round(downloadProgress)}%`}
              </Alert>
            )}

            {(previewing || previewError) && (
              <Alert tone={previewError ? 'error' : 'info'}>
                {previewError || 'Preparing preview...'}
              </Alert>
            )}

            {(uploading || uploadError) && (
              <Alert tone={uploadError ? 'error' : 'info'}>
                {uploadError || `Uploading ${uploadLabel || 'file'}... ${Math.round(uploadProgress)}%`}
              </Alert>
            )}

            {isFileShare && (
              <section className="rounded-lg border border-white/10 bg-gray-900/90">
                <div className="flex flex-wrap items-center justify-between gap-3 border-b border-white/10 px-4 py-3">
                  <div>
                    <h2 className="text-base font-semibold">Shared file</h2>
                    <p className="text-xs text-gray-400">
                      {canPreview ? 'Preview opens automatically when supported.' : 'Preview is not available for this share.'}
                    </p>
                  </div>
                  {canDownload && (
                    <button
                      className="rounded bg-cyan-500 px-3 py-1.5 text-sm font-medium text-gray-950 disabled:opacity-60"
                      onClick={downloadRootFile}
                      disabled={downloading}>
                      Download File
                    </button>
                  )}
                </div>

                {sharePreview && previewDataUrl && (
                  <div className="flex min-h-[18rem] items-center justify-center overflow-hidden bg-black/30 p-4">
                    {previewIsImage && (
                      // eslint-disable-next-line @next/next/no-img-element
                      <img
                        className="max-h-[52vh] max-w-full rounded object-contain shadow-2xl"
                        src={previewDataUrl}
                        alt={sharePreview.filename}
                      />
                    )}
                    {!previewIsImage && previewIsPdf && (
                      <object
                        className="h-[52vh] w-full max-w-4xl rounded bg-white"
                        data={previewDataUrl}
                        type={sharePreview.mime_type}
                        aria-label={sharePreview.filename}
                      />
                    )}
                    {!previewIsImage && !previewIsPdf && (
                      <div className="rounded border border-gray-800 bg-gray-950 p-6 text-sm text-gray-300">
                        Preview is not available for this file type.
                      </div>
                    )}
                  </div>
                )}
                {!sharePreview && canPreview && !previewError && (
                  <div className="m-4 rounded border border-gray-800 bg-gray-950 p-6 text-sm text-gray-300">
                    {previewing ? 'Preparing preview...' : 'Preview will appear here when available.'}
                  </div>
                )}
              </section>
            )}

            {canList ? (
              <FileDropOverlay disabled={!canUpload || !isDirectoryShare} disabledMessage="Upload is not available for this share">
                <UploadProgress />
                {files.length === 0 ? (
                  <section className="rounded border border-dashed border-gray-700 bg-gray-900 p-8 text-center text-gray-300">
                    This directory is empty.
                    {canUpload && isDirectoryShare && <div className="mt-2 text-sm text-cyan-200">Drop files here to add them.</div>}
                  </section>
                ) : (
                  <Filesystem files={files} />
                )}
              </FileDropOverlay>
            ) : (
              !isFileShare && (
                <Alert tone="info">
                  Directory listing is not available for this share. Use a link with list access to browse contents.
                </Alert>
              )
            )}
          </>
        )}

        {status === 'error' && (
          <section className="rounded border border-red-500/40 bg-red-950/40 p-6 text-red-100">
            <h2 className="text-xl font-semibold">Share unavailable</h2>
            <p className="mt-2 text-sm">{error || 'This share could not be opened. It may be expired, revoked, or disabled.'}</p>
            <button className="mt-4 rounded border border-red-300/50 px-3 py-2 text-sm hover:bg-red-900/40" onClick={() => openSession(token).catch(() => undefined)}>
              Retry
            </button>
          </section>
        )}
      </div>
    </main>
  )
}

export default SharePageClient
