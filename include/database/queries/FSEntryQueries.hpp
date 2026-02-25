#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <optional>
#include <pqxx/result>

namespace fs = std::filesystem;

namespace vh::fs::model {
    struct Entry;
}

namespace vh::database {

struct FSEntryQueries {
    static void updateFSEntry(const std::shared_ptr<fs::model::Entry>& entry);

    static std::shared_ptr<fs::model::Entry> getFSEntry(const std::string& base32);

    static std::shared_ptr<fs::model::Entry> getFSEntryByInode(ino_t ino);

    static std::shared_ptr<fs::model::Entry> getFSEntryById(unsigned int entryId);

    static std::vector<std::shared_ptr<fs::model::Entry>> listDir(const std::optional<unsigned int>& entryId, bool recursive = false);

    static void renameEntry(const std::shared_ptr<fs::model::Entry>& entry);

    [[nodiscard]] static ino_t getNextInode();

    [[nodiscard]] static bool rootExists();

    [[nodiscard]] static std::shared_ptr<fs::model::Entry> getRootEntry();

    [[nodiscard]] static pqxx::result collectParentChain(unsigned int parentId);
};

}
