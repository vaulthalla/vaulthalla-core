#pragma once

#include <memory>
#include <string>

namespace vh::types {
struct File;
}

namespace vh::sync::model {

struct EntryKey {
    std::u8string rel; // normalized relative path: no leading '/', consistent separators
    friend auto operator<=>(const EntryKey&, const EntryKey&) = default;
};

struct CompareResult {
    bool equalContent;
    bool localNewer;
    bool remoteNewer;
};

}
