#pragma once

#include "types/sync/helpers.hpp"

namespace vh::types::sync {

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
    std::shared_ptr<File> local;
    std::shared_ptr<File> remote;
    bool freeAfterDownload = false; // cache mode hint
};

}
