#include "FUSEPermissions.hpp"
#include <memory>
#include <sys/stat.h>

using namespace vh::fuse;

bool FUSEPermissions::hasPermission(const types::File& file, uid_t uid, gid_t gid, int mask) {
    mode_t mode = file.mode;

    bool owner = (uid == file.uid);
    bool group = (gid == file.gid) || (userGroups_.contains(uid) && userGroups_.at(uid).contains(file.gid));

    // Read
    if ((mask & R_OK) && !((owner && (mode & S_IRUSR)) || (group && (mode & S_IRGRP)) || (mode & S_IROTH)))
        return false;

    // Write
    if ((mask & W_OK) && !((owner && (mode & S_IWUSR)) || (group && (mode & S_IWGRP)) || (mode & S_IWOTH)))
        return false;

    // Exec
    if ((mask & X_OK) && !((owner && (mode & S_IXUSR)) || (group && (mode & S_IXGRP)) || (mode & S_IXOTH)))
        return false;

    return true;
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
