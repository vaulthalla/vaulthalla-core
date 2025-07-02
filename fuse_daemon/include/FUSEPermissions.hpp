#pragma once

#include "../../shared/include/types/db/file/File.hpp"
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>

namespace vh::fuse {

class FUSEPermissions {
  public:
    bool hasPermission(const types::File& file, uid_t uid, gid_t gid, int mask);

    void createUser(uid_t uid, const std::string& username);
    void createGroup(gid_t gid, const std::string& groupName);
    void assignUserToGroup(uid_t uid, gid_t gid);
    void removeUserFromGroup(uid_t uid, gid_t gid);

    std::unordered_set<gid_t> getGroupsForUser(uid_t uid) const;

  private:
    std::unordered_map<uid_t, std::string> users_;
    std::unordered_map<gid_t, std::string> groups_;
    std::unordered_map<uid_t, std::unordered_set<gid_t>> userGroups_;
};

} // namespace vh::fuse
