#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

namespace vh::core {

    class DirectoryWalker {
    public:
        struct Entry {
            fs::path path;
            bool is_directory;
            std::uintmax_t size;
            std::filesystem::file_time_type last_modified;
        };

        explicit DirectoryWalker(bool recursive = true);

        std::vector<Entry> walk(const fs::path& root,
                                std::function<bool(const fs::directory_entry&)> filter = nullptr) const;

    private:
        bool recursive;
    };

} // namespace vh::core
