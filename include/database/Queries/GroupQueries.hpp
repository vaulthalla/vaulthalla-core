#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace vh::types {
    struct Group;
}

namespace vh::database {

struct GroupQueries {
    GroupQueries() = default;

    static unsigned int createGroup(const std::shared_ptr<types::Group>& group);
    static void updateGroup(const std::shared_ptr<types::Group>& group);
    static void deleteGroup(unsigned int groupId);
    static void addMemberToGroup(unsigned int group, unsigned int member);
    static void addMembersToGroup(unsigned int group, const std::vector<unsigned int>& members);
    static void removeMemberFromGroup(unsigned int group, unsigned int member);
    static void removeMembersFromGroup(unsigned int group, const std::vector<unsigned int>& members);
    static std::shared_ptr<types::Group> getGroup(unsigned int groupId);
    static std::shared_ptr<types::Group> getGroupByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Group>> listGroups(unsigned int userId = 0);
    [[nodiscard]] static bool groupExists(const std::string& name);
};

}
