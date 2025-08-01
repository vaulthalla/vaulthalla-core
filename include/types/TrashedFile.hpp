#pragma once

#include <filesystem>
#include <optional>
#include <vector>
#include <boost/uuid/uuid.hpp>

namespace pqxx {
class row;
class result;
}

namespace fs = std::filesystem;

namespace vh::types {

struct TrashedFile {
    unsigned int id{}, vault_id{};
    fs::path fuse_path;
    boost::uuids::uuid uuid{};
    std::time_t trashed_at{}, trashed_by{};
    std::optional<std::time_t> deleted_at{};

    explicit TrashedFile(const pqxx::row& row);
};

std::vector<std::shared_ptr<TrashedFile>> trashed_files_from_pq_res(const pqxx::result& res);

}
