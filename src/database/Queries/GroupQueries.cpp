#include "database/Queries/GroupQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Group.hpp"

using namespace vh::database;

unsigned int GroupQueries::createGroup(const std::shared_ptr<types::Group>& group) {
    return Transactions::exec("GroupQueries::createGroup", [&](pqxx::work& txn) {
        pqxx::params p{group->name, group->description, group->linux_gid};
        const auto res = txn.exec_prepared("insert_group", p);
        if (res.empty()) throw std::runtime_error("Failed to create group: " + group->name);
        return res.one_field().as<unsigned int>();
    });
}

void GroupQueries::updateGroup(const std::shared_ptr<types::Group>& group) {
    Transactions::exec("GroupQueries::updateGroup", [&](pqxx::work& txn) {
        pqxx::params p{group->id, group->name, group->description, group->linux_gid};
        txn.exec_prepared("update_group", p);
    });
}

void GroupQueries::deleteGroup(const unsigned int groupId) {
    Transactions::exec("GroupQueries::deleteGroup", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_group", groupId);
    });
}

void GroupQueries::addMemberToGroup(const unsigned int group, const unsigned int member) {
    Transactions::exec("GroupQueries::addMemberToGroup", [&](pqxx::work& txn) {
        txn.exec_prepared("add_member_to_group", pqxx::params{group, member});
    });
}

void GroupQueries::addMembersToGroup(const unsigned int group, const std::vector<unsigned int>& members) {
    Transactions::exec("GroupQueries::addMembersToGroup", [&](pqxx::work& txn) {
        for (const auto& member : members)
            txn.exec_prepared("add_member_to_group", pqxx::params{group, member});
    });
}

void GroupQueries::removeMemberFromGroup(const unsigned int group, const unsigned int member) {
    Transactions::exec("GroupQueries::removeMemberFromGroup", [&](pqxx::work& txn) {
        txn.exec_prepared("remove_member_from_group", pqxx::params{group, member});
    });
}

void GroupQueries::removeMembersFromGroup(const unsigned int group, const std::vector<unsigned int>& members) {
    Transactions::exec("GroupQueries::removeMembersFromGroup", [&](pqxx::work& txn) {
        for (const auto& member : members)
            txn.exec_prepared("remove_member_from_group", pqxx::params{group, member});
    });
}

std::vector<std::shared_ptr<vh::types::Group>> GroupQueries::listGroups(unsigned int userId) {
    return Transactions::exec("GroupQueries::listGroups", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::Group>> {
        pqxx::result res;
        if (userId == 0) res = txn.exec_prepared("list_all_groups");
        else res = txn.exec_prepared("list_groups_by_user", userId);

        std::vector<std::shared_ptr<types::Group>> groups;
        for (const auto& group : res) {
            const auto groupId = group["id"].as<unsigned int>();
            const auto members = txn.exec_prepared("list_group_members", groupId);
            groups.push_back(std::make_shared<types::Group>(group, members));
        }
        return groups;
    });
}

std::shared_ptr<vh::types::Group> GroupQueries::getGroup(const unsigned int groupId) {
    return Transactions::exec("GroupQueries::getGroup", [&](pqxx::work& txn) -> std::shared_ptr<types::Group> {
        const auto res = txn.exec_prepared("get_group", groupId);
        if (res.empty()) return nullptr;
        const auto members = txn.exec_prepared("list_group_members", groupId);
        return std::make_shared<types::Group>(res.one_row(), members);
    });
}

std::shared_ptr<vh::types::Group> GroupQueries::getGroupByName(const std::string& name) {
    return Transactions::exec("GroupQueries::getGroupByName",
        [&](pqxx::work& txn) -> std::shared_ptr<types::Group> {
            const auto res = txn.exec_prepared("get_group_by_name", name);
            if (res.empty()) return nullptr;
            const auto groupRow = res.one_row();
            const auto groupId = groupRow["id"].as<unsigned int>();
            const auto members = txn.exec_prepared("list_group_members", groupId);
            return std::make_shared<types::Group>(groupRow, members);
        });
}
