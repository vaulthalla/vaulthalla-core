#pragma once

#include "concurrency/Task.hpp"

#include <memory>

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct File;
}

namespace vh::sync::model {
struct ScopedOp;
}

namespace vh::sync::tasks {

struct Upload final : concurrency::PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;
    model::ScopedOp& op;

    Upload(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 model::ScopedOp& op);

    void operator()() override;
};

}
