#pragma once

#include "db/model/ListQueryParams.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace vh::identities::model { struct Group; }

namespace vh::db::query::identities {

class Group {
    using G = vh::identities::model::Group;
    using GroupPtr = std::shared_ptr<G>;

public:
    Group() = default;

    static unsigned int createGroup(const GroupPtr& group);
    static void updateGroup(const GroupPtr& group);
    static void deleteGroup(unsigned int groupId);
    static void addMemberToGroup(unsigned int group, unsigned int member);
    static void addMembersToGroup(unsigned int group, const std::vector<unsigned int>& members);
    static void removeMemberFromGroup(unsigned int group, unsigned int member);
    static void removeMembersFromGroup(unsigned int group, const std::vector<unsigned int>& members);
    static GroupPtr getGroup(unsigned int groupId);
    static GroupPtr getGroupByName(const std::string& name);
    static std::vector<GroupPtr> listGroups(const std::optional<unsigned int>& userId = {}, model::ListQueryParams&& params = {});
    [[nodiscard]] static bool groupExists(const std::string& name);
};

}
