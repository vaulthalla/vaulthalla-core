#include "storage/LocalDiskStorageEngine.hpp"
#include "types/config/ConfigRegistry.hpp"
#include "types/db/Vault.hpp"
#include "types/db/Volume.hpp"
#include "types/db/File.hpp"
#include <fstream>

namespace vh::storage {

LocalDiskStorageEngine::LocalDiskStorageEngine(const std::shared_ptr<types::LocalDiskVault>& vault,
                                               const std::vector<std::shared_ptr<types::Volume> >& volumes)
    : StorageEngine(vault, volumes, types::config::ConfigRegistry::get().fuse.root_mount_path / vault->mount_point) {
        if (!std::filesystem::exists(root_)) std::filesystem::create_directories(root_);
        for (const auto& volume : volumes)
            if (volume)
                if (!std::filesystem::exists(root_ / volume->path_prefix))
                    std::filesystem::create_directories(
                        root_ / volume->path_prefix);
    }

    void LocalDiskStorageEngine::mountVolume(const std::shared_ptr<types::Volume>& volume) {
        if (!volume) return;

        if (!std::filesystem::exists(root_ / volume->path_prefix))
            std::filesystem::create_directories(
                root_ / volume->path_prefix);

        volumes_.push_back(volume);
    }

    void LocalDiskStorageEngine::unmountVolume(const std::shared_ptr<types::Volume>& volume) {
        if (!volume) return;
        std::erase(volumes_, volume);
    }

    bool LocalDiskStorageEngine::writeFile(const std::filesystem::path& rel_path,
                                           const std::vector<uint8_t>& data,
                                           const bool overwrite) {
        const auto full_path = root_ / rel_path;
        std::filesystem::create_directories(full_path.parent_path());

        if (!overwrite && std::filesystem::exists(full_path)) return false;

        std::ofstream out(full_path, std::ios::binary);
        if (!out) return false;

        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        return out.good();
    }

    std::optional<std::vector<uint8_t> > LocalDiskStorageEngine::readFile(const std::filesystem::path& rel_path) const {
        auto full_path = root_ / rel_path;
        std::ifstream in(full_path, std::ios::binary | std::ios::ate);
        if (!in) return std::nullopt;

        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) return std::nullopt;

        return buffer;
    }

    bool LocalDiskStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
        const auto full_path = root_ / rel_path;
        return std::filesystem::remove(full_path);
    }

    bool LocalDiskStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
        return std::filesystem::exists(root_ / rel_path);
    }

    std::vector<std::shared_ptr<types::File> > LocalDiskStorageEngine::listFilesInDir(
        const unsigned int volume_id, const std::filesystem::path& rel_path,
        const bool recursive) const {
        std::vector<std::shared_ptr<types::File> > files;

        const auto it = std::ranges::find_if(volumes_.begin(), volumes_.end(),
                                             [volume_id](const auto& v) { return v->id == volume_id; });

        if (it == volumes_.end()) throw std::runtime_error("Volume not found with ID: " + std::to_string(volume_id));

        const auto& volume = *it;

        const auto full_path = rel_path.empty() ? root_ / volume->path_prefix : root_ / volume->path_prefix / rel_path;

        if (!std::filesystem::exists(full_path) || !std::filesystem::is_directory(full_path)) return files;

        for (const auto& entry : std::filesystem::directory_iterator(full_path)) {
            auto f = std::make_shared<types::File>();
            const auto status = entry.symlink_status(); // safer for symlinks

            f->id = 0; // If you don't have DB IDs in this context, leave as 0 or assign some sentinel
            f->storage_volume_id = volume_id; // Same here, fill if meaningful
            f->parent_id = std::nullopt; // You can compute if needed

            f->name = entry.path().filename().string();
            f->is_directory = entry.is_directory();
            f->mode = static_cast<unsigned long long>(status.permissions());
            f->uid = 0;        // Could use stat() to fill real UID
            f->gid = 0;        // Could use stat() to fill real GID
            f->created_by = 0; // Application-specific

            auto ftime = std::filesystem::last_write_time(entry);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
            f->updated_at = std::chrono::system_clock::to_time_t(sctp);
            f->created_at = f->updated_at; // No portable created time; use updated_at as placeholder

            f->current_version_size_bytes = entry.is_regular_file() ? entry.file_size() : 0;
            f->is_trashed = false;
            f->trashed_by = 0;
            f->full_path = entry.path().string();

            files.push_back(f);

            if (recursive&& entry
            .
            is_directory()
            ) {
                auto sub_files = listFilesInDir(volume_id, rel_path / entry.path().filename(), true);
                files.insert(files.end(), sub_files.begin(), sub_files.end());
            }
        }

        return files;
    }

    std::filesystem::path LocalDiskStorageEngine::resolvePath(const std::string& id) const {
        return root_ / id;
    }

    std::filesystem::path LocalDiskStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path,
                                                                  const unsigned int volumeId) const {
        const auto it = std::ranges::find_if(volumes_.begin(), volumes_.end(),
                                             [volumeId](const auto& v) { return v->id == volumeId; });
        if (it == volumes_.end()) throw std::runtime_error("Volume not found with ID: " + std::to_string(volumeId));
        const auto& volume = *it;
        const auto& fuse_mnt = types::config::ConfigRegistry::get().fuse.root_mount_path;

        if (rel_path.empty()) return fuse_mnt / root_ / volume->path_prefix;

        std::filesystem::path safe_rel = rel_path;
        if (safe_rel.is_absolute()) safe_rel = safe_rel.lexically_relative("/");

        return fuse_mnt / root_ / volume->path_prefix / safe_rel;
    }

    fs::path LocalDiskStorageEngine::getRelativePath(const fs::path& absolute_path) const {
        if (absolute_path.is_absolute()) return std::filesystem::relative(absolute_path, root_);
        return absolute_path;
    }

    std::filesystem::path LocalDiskStorageEngine::getRootPath() const {
        return root_;
    }

} // namespace vh::storage