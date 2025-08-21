#pragma once

#include "concurrency/Task.hpp"

#include <memory>

namespace fs = std::filesystem;

namespace vh::storage {
    class StorageEngine;
}

namespace vh::types {
    struct TrashedFile;
}

namespace vh::concurrency {

struct LocalDeleteTask final : PromisedTask {
    std::shared_ptr<storage::StorageEngine> engine;
    std::shared_ptr<types::TrashedFile> file;

    LocalDeleteTask(std::shared_ptr<storage::StorageEngine> eng, std::shared_ptr<types::TrashedFile> f);

    void operator()() override;
};

}
