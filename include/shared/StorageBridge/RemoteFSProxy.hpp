#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstddef>

namespace vh::types {
    class File;
}

namespace vh::shared::bridge {

    class UnifiedStorage;

    class RemoteFSProxy {
    public:
        explicit RemoteFSProxy(std::shared_ptr<UnifiedStorage> backend);

        [[nodiscard]] bool fileExists(const std::string& path) const;
        [[nodiscard]] types::File stat(const std::string& path) const;
        [[nodiscard]] std::vector<types::File> listDirectory(const std::string& path) const;

        ssize_t readFile(const std::string& path, char* buf, size_t size, off_t offset);
        ssize_t writeFile(const std::string& path, const char* buf, size_t size, off_t offset);

        bool mkdir(const std::string& path);
        bool unlink(const std::string& path);
        bool rename(const std::string& oldPath, const std::string& newPath);

    private:
        std::shared_ptr<UnifiedStorage> backend_;
    };

}
