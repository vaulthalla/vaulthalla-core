#pragma once

#include "concurrency/Task.hpp"

#include <memory>

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct File;
}

namespace vh::concurrency {

struct UploadTask final : PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;

    UploadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f);

    void operator()() override;
};

}
