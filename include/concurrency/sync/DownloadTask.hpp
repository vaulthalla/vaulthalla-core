#pragma once

#include "concurrency/Task.hpp"

#include <memory>

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct File;

namespace sync {
struct ScopedOp;
}

}

namespace vh::concurrency {

struct DownloadTask final : PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;
    types::sync::ScopedOp& op;
    bool freeAfterDownload;

    DownloadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 types::sync::ScopedOp& op,
                 bool freeAfter = false);

    void operator()() override;
};

}
