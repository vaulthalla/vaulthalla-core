#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <string>

namespace vh::types {

struct FileIcon {
    unsigned int file_id;
    std::string name;
    std::optional<std::string> icon_path;
    std::optional<std::string> thumbnail_path;
    std::optional<std::string> preview_path;
    std::time_t generated_at;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::FileIcon, (), (file_id, name, icon_path, thumbnail_path, preview_path, generated_at))
