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

    static std::shared_ptr<types::FSEntry> getFSEntryById(unsigned int entryId);

    static std::vector<std::shared_ptr<types::FSEntry>> listDir(const fs::path& absPath, bool recursive = false);

    [[nodiscard]] static bool exists(const fs::path& absPath);

    static void renameEntry(const std::shared_ptr<types::FSEntry>& entry);
};

}
