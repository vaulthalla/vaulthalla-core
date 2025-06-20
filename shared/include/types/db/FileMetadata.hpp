#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <string>

namespace vh::types {
enum class FileMetadataType { String, Integer, Boolean, Timestamp, Float };

struct FileMetadata {
    unsigned int id;
    unsigned int file_id;
    std::string key;
    FileMetadataType type;
};

struct FileMetadataString {
    unsigned int file_metadata_id;
    std::string value;
};

struct FileMetadataInteger {
    unsigned int file_metadata_id;
    unsigned long value;
};

struct FileMetadataBoolean {
    unsigned int file_metadata_id;
    bool value;
};

struct FileMetadataTimestamp {
    unsigned int file_metadata_id;
    std::time_t value;
};

struct FileMetadataFloat {
    unsigned int file_metadata_id;
    double value;
};

} // namespace vh::types

BOOST_DESCRIBE_ENUM(vh::types::FileMetadataType, String, Integer, Boolean, Timestamp, Float)

BOOST_DESCRIBE_STRUCT(vh::types::FileMetadata, (), (id, file_id, key, type))

BOOST_DESCRIBE_STRUCT(vh::types::FileMetadataString, (), (file_metadata_id, value))

BOOST_DESCRIBE_STRUCT(vh::types::FileMetadataInteger, (), (file_metadata_id, value))

BOOST_DESCRIBE_STRUCT(vh::types::FileMetadataBoolean, (), (file_metadata_id, value))

BOOST_DESCRIBE_STRUCT(vh::types::FileMetadataTimestamp, (), (file_metadata_id, value))

BOOST_DESCRIBE_STRUCT(vh::types::FileMetadataFloat, (), (file_metadata_id, value))
