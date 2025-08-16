#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <optional>

namespace fs = std::filesystem;

namespace vh::types {
    struct FSEntry;
}

namespace vh::database {

struct FSEntryQueries {
    static void updateFSEntry(const std::shared_ptr<types::FSEntry>& entry);

    static std::shared_ptr<types::FSEntry> getFSEntry(const std::string& base32);

    static std::shared_ptr<types::FSEntry> getFSEntryByInode(ino_t ino);

    static std::shared_ptr<types::FSEntry> getFSEntryById(unsigned int entryId);

    static std::vector<std::shared_ptr<types::FSEntry>> listDir(const std::optional<unsigned int>& entryId, bool recursive = false);

    static void renameEntry(const std::shared_ptr<types::FSEntry>& entry);

    [[nodiscard]] static ino_t getNextInode();

    [[nodiscard]] static bool rootExists();

    [[nodiscard]] static std::shared_ptr<types::FSEntry> getRootEntry();
};

}
