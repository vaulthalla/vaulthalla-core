#include "core/FileMetadata.hpp"
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <utility>

namespace fs = std::filesystem;

namespace vh::core {

    FileMetadata::FileMetadata(const fs::directory_entry& entry) {
        path = entry.path().string();
        size_bytes = entry.is_regular_file() ? entry.file_size() : 0;
        permissions = entry.status().permissions();

        struct stat info{};
        if (stat(entry.path().c_str(), &info) == 0) {
            created_at = std::chrono::system_clock::from_time_t(info.st_ctime);
            modified_at = std::chrono::system_clock::from_time_t(info.st_mtime);
            accessed_at = std::chrono::system_clock::from_time_t(info.st_atime);

            struct passwd *pw = getpwuid(info.st_uid);
            struct group *gr = getgrgid(info.st_gid);
            owner = pw ? pw->pw_name : std::to_string(info.st_uid);
            group = gr ? gr->gr_name : std::to_string(info.st_gid);
        } else {
            created_at = modified_at = accessed_at = std::chrono::system_clock::now();
            owner = group = "unknown";
        }
    }

    FileMetadata::FileMetadata(std::string  id,
                 const std::filesystem::path& path,
                 uintmax_t size)
            : path(path),
              size_bytes(size),
              id(std::move(id)),
              modified_at(std::chrono::system_clock::now()),
              created_at(modified_at),
              accessed_at(modified_at) {}

    nlohmann::json FileMetadata::to_json() const {
        return {
                {"id", id},
                {"path", path},
                {"size_bytes", size_bytes},
                {"permissions", static_cast<int>(permissions)},
                {"owner", owner},
                {"group", group},
                {"created_at", std::chrono::system_clock::to_time_t(created_at)},
                {"modified_at", std::chrono::system_clock::to_time_t(modified_at)},
                {"accessed_at", std::chrono::system_clock::to_time_t(accessed_at)}
        };
    }

    FileMetadata FileMetadata::from_json(const nlohmann::json& j) {
        FileMetadata meta;
        if (j.contains("id")) meta.id = j.at("id").get<std::string>();
        meta.path = j.at("path").get<std::string>();
        meta.size_bytes = j.at("size_bytes").get<uintmax_t>();
        meta.permissions = static_cast<std::filesystem::perms>(j.at("permissions").get<int>());

        meta.owner = j.at("owner").get<std::string>();
        meta.group = j.at("group").get<std::string>();

        meta.created_at = std::chrono::system_clock::from_time_t(j.at("created_at").get<time_t>());
        meta.modified_at = std::chrono::system_clock::from_time_t(j.at("modified_at").get<time_t>());
        meta.accessed_at = std::chrono::system_clock::from_time_t(j.at("accessed_at").get<time_t>());

        return meta;
    }

} // namespace vh::core
