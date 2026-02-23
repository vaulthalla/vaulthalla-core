#pragma once

#include "concurrency/Task.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace vh::storage { class StorageEngine; class CloudStorageEngine; }

namespace vh::types { struct File; }

namespace vh::types::sync { struct RemotePolicy; }

namespace vh::concurrency {

struct RotateKeyTask final : PromisedTask {
    std::shared_ptr<storage::StorageEngine> engine;
    std::shared_ptr<storage::CloudStorageEngine> cloud{nullptr};
    std::vector<std::shared_ptr<types::File>> files;
    std::size_t begin{};
    std::size_t end{};

    RotateKeyTask(std::shared_ptr<storage::StorageEngine> eng,
                  const std::vector<std::shared_ptr<types::File>>& f,
                  std::size_t begin_, std::size_t end_);

    void operator()() override;

private:
    using FileSP = std::shared_ptr<types::File>;
    using RemotePolicySP = std::shared_ptr<types::sync::RemotePolicy>;

    bool shouldSkipLocalWriteInCacheMode(const RemotePolicySP& policy, std::size_t ciphertextSize) const;

    std::vector<uint8_t> produceCiphertext(const FileSP& file,
                                           const std::vector<uint8_t>& buffer,
                                           bool bufferIsEncrypted) const;

    void hydrateIvAndVersionForRemoteEncrypted(const FileSP& file) const;

    void maybeWriteLocal(const RemotePolicySP& policy,
                         const FileSP& file,
                         const std::vector<uint8_t>& ciphertext) const;

    void rotateLocalFile(const FileSP& file) const;

    void rotateCloudFile(const RemotePolicySP& remotePolicy,
                         const FileSP& file) const;
};

}
