#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vh::types {
    struct Group;
}

namespace vh::database {

struct GroupQueries {
    GroupQueries() = default;

    static void createGroup(const std::string& name, const std::string& description = "");
    static void deleteGroup(unsigned int groupId);
    static void addMemberToGroup(unsigned int groupId, const std::string& name);
    static void removeMemberFromGroup(unsigned int groupId, unsigned int userId);
    static void updateGroup(unsigned int groupId, const std::string& newName);
    static std::vector<std::shared_ptr<types::Group>> listGroups();
    static std::shared_ptr<types::Group> getGroup(unsigned int groupId);
    static std::shared_ptr<types::Group> getGroupByName(const std::string& name);
    static void addStorageVolumeToGroup(unsigned int groupId, unsigned int volumeId);
    static void removeStorageVolumeFromGroup(unsigned int groupId, unsigned int volumeId);
    static std::vector<std::shared_ptr<types::Group>> listGroupsByUser(unsigned int userId);
    static std::vector<std::shared_ptr<types::Group>> listGroupsByStorageVolume(unsigned int volumeId);
};

}
