#pragma once

#include "helpers.hpp"

namespace vh::types {
struct File;
}

namespace vh::sync::model {

enum class ActionType {
    EnsureDirectories,
    Upload,
    Download,
    DeleteLocal,
    DeleteRemote,
};

struct Action {
    ActionType type;
    EntryKey key;
    std::shared_ptr<types::File> local;
    std::shared_ptr<types::File> remote;
    bool freeAfterDownload = false; // cache mode hint
};

}
