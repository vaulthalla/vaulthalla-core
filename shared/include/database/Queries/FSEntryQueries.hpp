#pragma once

#include <filesystem>
#include <memory>
#include <vector>

namespace fs = std::filesystem;

namespace vh::types {
    struct FSEntry;
}

namespace vh::database {

struct FSEntryQueries {
    static std::shared_ptr<types::FSEntry> getFSEntry(const fs::path& absPath);

    static std::vector<std::shared_ptr<types::FSEntry>> listDir(const fs::path& absPath, bool recursive = false);

    [[nodiscard]] static bool exists(const fs::path& absPath);
};

}
