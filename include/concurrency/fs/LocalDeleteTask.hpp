#pragma once

#include "concurrency/Task.hpp"

#include <memory>

namespace fs = std::filesystem;

namespace vh::storage {
    class StorageEngine;
}

namespace vh::types {
    struct TrashedFile;

namespace sync {
struct ScopedOp;
}

}

namespace vh::concurrency {

struct LocalDeleteTask final : PromisedTask {
    std::shared_ptr<storage::StorageEngine> engine;
    std::shared_ptr<types::TrashedFile> file;
    types::sync::ScopedOp& op;

    LocalDeleteTask(std::shared_ptr<storage::StorageEngine> eng, std::shared_ptr<types::TrashedFile> f, types::sync::ScopedOp& op);

    void operator()() override;
};

}
