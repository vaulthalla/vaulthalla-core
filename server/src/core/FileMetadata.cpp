#include "core/FileMetadata.hpp"
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

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

    nlohmann::json FileMetadata::to_json() const {
        return {
                {"path", path},
                {"size_bytes", size_bytes},
                {"permissions", static_cast<int>(permissions)},
                {"owner", owner},
                {"group", group},
                {"created_at", std::chrono::duration_cast<std::chrono::seconds>(created_at.time_since_epoch()).count()},
                {"modified_at", std::chrono::duration_cast<std::chrono::seconds>(modified_at.time_since_epoch()).count()},
                {"accessed_at", std::chrono::duration_cast<std::chrono::seconds>(accessed_at.time_since_epoch()).count()}
        };
    }

    FileMetadata FileMetadata::from_json(const nlohmann::json& j) {
        FileMetadata meta;
        meta.path = j.at("path").get<std::string>();
        meta.size_bytes = j.at("size_bytes").get<uintmax_t>();
        meta.permissions = static_cast<fs::perms>(j.at("permissions").get<int>());
        meta.owner = j.at("owner").get<std::string>();
        meta.group = j.at("group").get<std::string>();

        meta.created_at = std::chrono::system_clock::time_point(std::chrono::seconds(j.at("created_at").get<long long>()));
        meta.modified_at = std::chrono::system_clock::time_point(std::chrono::seconds(j.at("modified_at").get<long long>()));
        meta.accessed_at = std::chrono::system_clock::time_point(std::chrono::seconds(j.at("accessed_at").get<long long>()));

        return meta;
    }

} // namespace vh::core
