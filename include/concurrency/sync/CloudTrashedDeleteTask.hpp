#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <filesystem>

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct TrashedFile;

namespace sync {
    struct ScopedOp;
}

}

namespace vh::concurrency {

struct CloudTrashedDeleteTask final : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::TrashedFile> file;
    types::sync::ScopedOp& op;
    Type type{Type::PURGE};

    CloudTrashedDeleteTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::TrashedFile> f,
                 types::sync::ScopedOp& op,
                 const Type& type = Type::PURGE);

    void operator()() override;
};

}
