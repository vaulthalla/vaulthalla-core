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

struct CloudDeleteTask final : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;
    types::sync::ScopedOp& op;
    Type type{Type::PURGE};

    CloudDeleteTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 types::sync::ScopedOp& op,
                 const Type& type = Type::PURGE);

    void operator()() override;
};

}
