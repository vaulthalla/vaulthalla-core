#include "database/Queries/GroupQueries.hpp"
#include "database/Transactions.hpp"
#include "types/entities/Group.hpp"

using namespace vh::database;
using namespace vh::types;

unsigned int GroupQueries::createGroup(const std::shared_ptr<Group>& group) {
    return Transactions::exec("GroupQueries::createGroup", [&](pqxx::work& txn) {
        pqxx::params p{group->name, group->description, group->linux_gid};
        const auto res = txn.exec(pqxx::prepped{"insert_group"}, p);
        if (res.empty()) throw std::runtime_error("Failed to create group: " + group->name);
        return res.one_field().as<unsigned int>();
    });
}

void GroupQueries::updateGroup(const std::shared_ptr<Group>& group) {
    Transactions::exec("GroupQueries::updateGroup", [&](pqxx::work& txn) {
        pqxx::params p{group->id, group->name, group->description, group->linux_gid};
        txn.exec(pqxx::prepped{"update_group"}, p);
    });
}

void GroupQueries::deleteGroup(const unsigned int groupId) {
    Transactions::exec("GroupQueries::deleteGroup", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_group"}, groupId);
    });
}

void GroupQueries::addMemberToGroup(const unsigned int group, const unsigned int member) {
    Transactions::exec("GroupQueries::addMemberToGroup", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"add_member_to_group"}, pqxx::params{group, member});
    });
}

void GroupQueries::addMembersToGroup(const unsigned int group, const std::vector<unsigned int>& members) {
    Transactions::exec("GroupQueries::addMembersToGroup", [&](pqxx::work& txn) {
        for (const auto& member : members)
            txn.exec(pqxx::prepped{"add_member_to_group"}, pqxx::params{group, member});
    });
}

void GroupQueries::removeMemberFromGroup(const unsigned int group, const unsigned int member) {
    Transactions::exec("GroupQueries::removeMemberFromGroup", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"remove_member_from_group"}, pqxx::params{group, member});
    });
}

void GroupQueries::removeMembersFromGroup(const unsigned int group, const std::vector<unsigned int>& members) {
    Transactions::exec("GroupQueries::removeMembersFromGroup", [&](pqxx::work& txn) {
        for (const auto& member : members)
            txn.exec(pqxx::prepped{"remove_member_from_group"}, pqxx::params{group, member});
    });
}

std::vector<std::shared_ptr<Group>> GroupQueries::listGroups(const std::optional<unsigned int>& userId, ListQueryParams&& params) {
    return Transactions::exec("GroupQueries::listGroups", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<Group>> {
        std::string sql;

        if (!userId) {
            sql = appendPaginationAndFilter(
                "SELECT * FROM groups",
                params, "id", "name"
            );
        } else {
            sql = appendPaginationAndFilter(
                "SELECT g.* FROM groups g "
                "JOIN group_members gm ON g.id = gm.group_id "
                "WHERE gm.user_id = " + txn.quote(*userId),
                params, "g.id", "g.name"
            );
        }

        const auto res = txn.exec(sql);

        std::vector<std::shared_ptr<Group>> groups;
        for (const auto& group : res) {
            const auto groupId = group["id"].as<unsigned int>();
            const auto members = txn.exec(pqxx::prepped{"list_group_members"}, groupId);
            groups.push_back(std::make_shared<Group>(group, members));
        }
        return groups;
    });
}

std::shared_ptr<Group> GroupQueries::getGroup(const unsigned int groupId) {
    return Transactions::exec("GroupQueries::getGroup", [&](pqxx::work& txn) -> std::shared_ptr<Group> {
        const auto res = txn.exec(pqxx::prepped{"get_group"}, groupId);
        if (res.empty()) return nullptr;
        const auto members = txn.exec(pqxx::prepped{"list_group_members"}, groupId);
        return std::make_shared<Group>(res.one_row(), members);
    });
}

std::shared_ptr<Group> GroupQueries::getGroupByName(const std::string& name) {
    return Transactions::exec("GroupQueries::getGroupByName",
        [&](pqxx::work& txn) -> std::shared_ptr<Group> {
            const auto res = txn.exec(pqxx::prepped{"get_group_by_name"}, name);
            if (res.empty()) return nullptr;
            const auto groupRow = res.one_row();
            const auto groupId = groupRow["id"].as<unsigned int>();
            const auto members = txn.exec(pqxx::prepped{"list_group_members"}, groupId);
            return std::make_shared<Group>(groupRow, members);
        });
}

bool GroupQueries::groupExists(const std::string& name) {
    return Transactions::exec("GroupQueries::groupExists", [&](pqxx::work& txn) {
        const auto res = txn.exec(pqxx::prepped{"group_exists"}, name);
        if (res.empty()) return false;
        return res.one_field().as<bool>();
    });
}
