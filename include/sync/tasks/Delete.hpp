#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <variant>

namespace vh::storage {
class StorageEngine;
class CloudStorageEngine;
}

namespace vh::types {
struct File;
struct TrashedFile;
}

namespace vh::sync::model {
struct ScopedOp;
}

namespace vh::sync::tasks {

struct Delete final : concurrency::PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    using Target = std::variant<
        std::shared_ptr<types::File>,
        std::shared_ptr<types::TrashedFile>
    >;

    std::shared_ptr<storage::StorageEngine> engine;
    Target target;
    model::ScopedOp& op;
    Type type{Type::PURGE};

    Delete(std::shared_ptr<storage::StorageEngine> eng,
               Target tgt,
               model::ScopedOp& op,
               Type type = Type::PURGE);

    void operator()() override;
};

}
