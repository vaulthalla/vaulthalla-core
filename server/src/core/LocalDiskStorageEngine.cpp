#include "core/LocalDiskStorageEngine.hpp"

#include <utility>

namespace core {

    LocalDiskStorageEngine::LocalDiskStorageEngine(std::filesystem::path  root_dir)
            : root(std::move(root_dir)) {
        std::filesystem::create_directories(root);
    }

    bool LocalDiskStorageEngine::writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) {
        auto full_path = root / rel_path;
        std::filesystem::create_directories(full_path.parent_path());

        std::ofstream out(full_path, std::ios::binary);
        if (!out) return false;

        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        return out.good();
    }

    std::optional<std::vector<uint8_t>> LocalDiskStorageEngine::readFile(const std::filesystem::path& rel_path) const {
        auto full_path = root / rel_path;
        std::ifstream in(full_path, std::ios::binary | std::ios::ate);
        if (!in) return std::nullopt;

        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) return std::nullopt;

        return buffer;
    }

    bool LocalDiskStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
        auto full_path = root / rel_path;
        return std::filesystem::remove(full_path);
    }

}
