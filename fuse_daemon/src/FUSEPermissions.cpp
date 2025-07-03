#include "FUSEPermissions.hpp"

using namespace vh::fuse;

bool FUSEPermissions::hasPermission(const types::File& file, uid_t uid, gid_t gid, int mask) {
    // TODO: If needed, implement permission checks based on file metadata
    return true; // Default to true for now
}

void FUSEPermissions::createUser(uid_t uid, const std::string& username) {
    users_[uid] = username;
}

void FUSEPermissions::createGroup(gid_t gid, const std::string& groupName) {
    groups_[gid] = groupName;
}

void FUSEPermissions::assignUserToGroup(uid_t uid, gid_t gid) {
    userGroups_[uid].insert(gid);
}

void FUSEPermissions::removeUserFromGroup(uid_t uid, gid_t gid) {
    if (userGroups_.contains(uid)) {
        userGroups_[uid].erase(gid);
    }
}

std::unordered_set<gid_t> FUSEPermissions::getGroupsForUser(uid_t uid) const {
    if (userGroups_.contains(uid)) return userGroups_.at(uid);
    return {};
}
