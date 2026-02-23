#pragma once

#include "concurrency/Task.hpp"

#include <memory>

namespace vh::storage {
class StorageEngine;
class CloudStorageEngine;
}

namespace vh::types {
struct TrashedFile;

namespace sync {
struct ScopedOp;
}

}

namespace vh::concurrency {

struct DeleteTask final : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::StorageEngine> engine;
    std::shared_ptr<types::TrashedFile> file;
    types::sync::ScopedOp& op;
    Type type{Type::PURGE};

    DeleteTask(std::shared_ptr<storage::StorageEngine> eng,
                 std::shared_ptr<types::TrashedFile> f,
                 types::sync::ScopedOp& op,
                 const Type& type = Type::PURGE);

    void operator()() override;
};

}
