#pragma once

#include <boost/describe.hpp>
#include <string>

namespace vh::types {
enum class PermissionName {
    ManageUsers,
    ManageRoles,
    ManageStorage,
    ManageFiles,
    ViewAuditLog,
    UploadFile,
    DownloadFile,
    DeleteFile,
    ShareFile,
    LockFile
};

struct Permission {
    unsigned int id;
    PermissionName name;
    std::string description;
};
} // namespace vh::types

BOOST_DESCRIBE_ENUM(vh::types::PermissionName, ManageUsers, ManageRoles, ManageStorage, ManageFiles, ViewAuditLog,
                    UploadFile, DownloadFile, DeleteFile, ShareFile, LockFile)

BOOST_DESCRIBE_STRUCT(vh::types::Permission, (), (id, name, description))
