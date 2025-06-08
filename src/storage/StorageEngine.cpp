#include "include/storage/StorageEngine.hpp"
#include <fstream>
#include <iostream>

namespace vh::storage {

    StorageEngine::StorageEngine(const fs::path& root_directory)
            : root(fs::absolute(root_directory)) {
        if (!fs::exists(root)) {
            fs::create_directories(root);
        }
    }

    bool StorageEngine::writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data, bool overwrite) {
        fs::path full_path = root / relative_path;

        if (fs::exists(full_path) && !overwrite) {
            std::cerr << "File already exists and overwrite is false: " << full_path << '\n';
            return false;
        }

        try {
            fs::create_directories(full_path.parent_path());
            std::ofstream out(full_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            return out.good();
        } catch (const std::exception& e) {
            std::cerr << "Failed to write file: " << full_path << " — " << e.what() << '\n';
            return false;
        }
    }

    std::optional<std::vector<uint8_t>> StorageEngine::readFile(const fs::path& relative_path) const {
        fs::path full_path = root / relative_path;

        if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
            std::cerr << "File not found or not a regular file: " << full_path << '\n';
            return std::nullopt;
        }

        try {
            std::ifstream in(full_path, std::ios::binary | std::ios::ate);
            std::streamsize size = in.tellg();
            in.seekg(0, std::ios::beg);

            std::vector<uint8_t> buffer(size);
            if (in.read(reinterpret_cast<char*>(buffer.data()), size)) {
                return buffer;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to read file: " << full_path << " — " << e.what() << '\n';
        }

        return std::nullopt;
    }

    bool StorageEngine::deleteFile(const fs::path& relative_path) {
        fs::path full_path = root / relative_path;

        try {
            return fs::remove(full_path);
        } catch (const std::exception& e) {
            std::cerr << "Failed to delete file: " << full_path << " — " << e.what() << '\n';
            return false;
        }
    }

    bool StorageEngine::fileExists(const fs::path& relative_path) const {
        return fs::exists(root / relative_path);
    }

    fs::path StorageEngine::getAbsolutePath(const fs::path& relative_path) const {
        return fs::absolute(root / relative_path);
    }

    fs::path StorageEngine::getRootPath() const { return root; }

} // namespace vh::storage
