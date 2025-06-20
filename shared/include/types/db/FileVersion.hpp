#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <string>

namespace vh::types {
struct FileVersion {
    unsigned int id;
    unsigned int file_id;
    unsigned int version_number;
    std::string content_hash;
    unsigned long size_bytes;
    std::string mime_type;
    std::string storage_path;
    std::time_t created_at;
    unsigned int created_by;
};
} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::FileVersion, (),
                      (id, file_id, version_number, content_hash, size_bytes, mime_type, storage_path, created_at,
                       created_by))
