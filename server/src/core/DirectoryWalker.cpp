#include "core/DirectoryWalker.hpp"
#include <iostream>
#include <variant>

using DirectoryIteratorVariant = std::variant<
        std::filesystem::directory_iterator,
        std::filesystem::recursive_directory_iterator
>;

namespace core {

    DirectoryWalker::DirectoryWalker(bool recursive)
            : recursive(recursive) {}

    std::vector<DirectoryWalker::Entry> DirectoryWalker::walk(
            const fs::path& root,
            std::function<bool(const fs::directory_entry&)> filter) const
    {
        std::vector<Entry> entries;

        if (!fs::exists(root) || !fs::is_directory(root)) {
            std::cerr << "Invalid directory path: " << root << '\n';
            return entries;
        }

        DirectoryIteratorVariant it = recursive
                                      ? DirectoryIteratorVariant{std::filesystem::recursive_directory_iterator(root)}
                                      : DirectoryIteratorVariant{std::filesystem::directory_iterator(root)};

        std::visit([&filter, &entries](auto&& dir_iter) {
            for (const auto& dir_entry : dir_iter) {
                try {
                    if (filter && !filter(dir_entry)) continue;

                    Entry entry {
                            dir_entry.path(),
                            dir_entry.is_directory(),
                            dir_entry.is_regular_file() ? dir_entry.file_size() : 0,
                            fs::last_write_time(dir_entry)
                    };

                    entries.push_back(entry);
                } catch (const std::exception& e) {
                    std::cerr << "Error accessing: " << dir_entry.path() << " — " << e.what() << '\n';
                }
            }
        }, it);

        return entries;
    }

} // namespace core
