'use client'

import { FileSystem } from '@/components/files/FileSystem'
import { useFSStore } from '@/stores/fsStore'
import { FileDropOverlay } from '@/components/files/FileDropOverlay'
import { FileWithRelativePath } from '@/models/systemFile'
import UploadProgress from '@/components/loading/UploadProgress'
import React from 'react'

const FSClientPage = () => {
  const { files, upload, setPath, fetchFiles } = useFSStore()

  const processFiles = (files: FileWithRelativePath[]) => {
    ;(async () => {
      upload(files).catch(console.error)
      await fetchFiles()
    })()
  }

  return (
    <FileDropOverlay onFiles={processFiles}>
      <UploadProgress />
      <FileSystem files={files} onNavigate={setPath} />
    </FileDropOverlay>
  )
}

export default FSClientPage
