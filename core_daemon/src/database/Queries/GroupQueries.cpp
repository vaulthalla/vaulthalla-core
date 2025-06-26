#include "database/Queries/GroupQueries.hpp"
#include "database/Transactions.hpp"
#include "types/db/Group.hpp"
#include "types/db/StorageVolume.hpp"

using namespace vh::database;

void GroupQueries::createGroup(const std::string& name, const std::string& description) {
    Transactions::exec("GroupQueries::createGroup",
        [&](pqxx::work& txn) {
            txn.exec("INSERT INTO groups (name, description) VALUES ("
                + txn.quote(name) + ", " + txn.quote(description) + ")");});
}

void GroupQueries::deleteGroup(const unsigned int groupId) {
    Transactions::exec("GroupQueries::deleteGroup",
        [&](pqxx::work& txn) {
            txn.exec("DELETE FROM groups WHERE id = " + txn.quote(groupId));
        });
}

void GroupQueries::addMemberToGroup(const unsigned int groupId, const std::string& name) {
    Transactions::exec("GroupQueries::addMemberToGroup",
        [&](pqxx::work& txn) {
            txn.exec(
                "INSERT INTO group_members (group_id, user_id, joined_at) "
                "VALUES (" + txn.quote(groupId) + ", (SELECT id FROM users WHERE name = " + txn.quote(name) + "), NOW())");
        });
}

void GroupQueries::removeMemberFromGroup(const unsigned int groupId, const unsigned int userId) {
    Transactions::exec("GroupQueries::removeMemberFromGroup",
        [&](pqxx::work& txn) {
            txn.exec("DELETE FROM group_members WHERE group_id = " + txn.quote(groupId) +
                     " AND user_id = " + txn.quote(userId));
        });
}

void GroupQueries::updateGroup(const unsigned int groupId, const std::string& newName) {
    Transactions::exec("GroupQueries::updateGroup",
        [&](pqxx::work& txn) {
            txn.exec("UPDATE groups SET name = " + txn.quote(newName) + " WHERE id = " + txn.quote(groupId));
        });
}

std::vector<std::shared_ptr<vh::types::Group>> GroupQueries::listGroups() {
    return Transactions::exec("GroupQueries::listGroups",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<vh::types::Group>> {
            const auto res = txn.exec("SELECT * FROM groups");

            std::vector<std::shared_ptr<types::Group>> groups;
            for (const auto& group : res) {
                const auto groupId = group["id"].as<unsigned int>();

                const auto members = txn.exec(
                    "SELECT u.*, gm.joined_at "
                    "FROM users u "
                    "JOIN group_members gm ON u.id = gm.user_id "
                    "WHERE gm.group_id = " + txn.quote(groupId));

                const auto storageVolumes = txn.exec(
                "SELECT v.*, gsv.assigned_at "
                "FROM storage_volumes v "
                "JOIN group_storage_volumes gsv ON v.id = gsv.volume_id "
                "WHERE gsv.group_id = " + txn.quote(groupId));

                groups.push_back(std::make_shared<types::Group>(group, members, storageVolumes));
            }
            return groups;
        });
}

std::shared_ptr<vh::types::Group> GroupQueries::getGroup(const unsigned int groupId) {
    return Transactions::exec("GroupQueries::getGroup",
        [&](pqxx::work& txn) -> std::shared_ptr<vh::types::Group> {
            const auto groupRow = txn.exec("SELECT * FROM groups WHERE id = " + txn.quote(groupId)).one_row();

            const auto members = txn.exec(
                "SELECT u.*, gm.joined_at "
                "FROM users u "
                "JOIN group_members gm ON u.id = gm.user_id "
                "WHERE gm.group_id = " + txn.quote(groupId));

            const auto storageVolumes = txn.exec(
                "SELECT v.*, gsv.assigned_at "
                "FROM storage_volumes v "
                "JOIN group_storage_volumes gsv ON v.id = gsv.volume_id "
                "WHERE gsv.group_id = " + txn.quote(groupId));

            return std::make_shared<types::Group>(groupRow, members, storageVolumes);
        });
}

std::shared_ptr<vh::types::Group> GroupQueries::getGroupByName(const std::string& name) {
    return Transactions::exec("GroupQueries::getGroupByName",
        [&](pqxx::work& txn) -> std::shared_ptr<vh::types::Group> {
            const auto groupRow = txn.exec(
                "SELECT * FROM groups WHERE name = " + txn.quote(name)).one_row();

            const auto groupId = groupRow["id"].as<unsigned int>();

            const auto members = txn.exec(
                "SELECT u.*, gm.joined_at "
                "FROM users u "
                "JOIN group_members gm ON u.id = gm.user_id "
                "WHERE gm.group_id = " + txn.quote(groupRow["id"].as<unsigned int>()));

            const auto storageVolumes = txn.exec(
                "SELECT v.*, gsv.assigned_at "
                "FROM storage_volumes v "
                "JOIN group_storage_volumes gsv ON v.id = gsv.volume_id "
                "WHERE gsv.group_id = " + txn.quote(groupId));

            return std::make_shared<types::Group>(groupRow, members, storageVolumes);
        });
}

void GroupQueries::addStorageVolumeToGroup(const unsigned int groupId, const unsigned int volumeId) {
    Transactions::exec("GroupQueries::addStorageVolumeToGroup",
        [&](pqxx::work& txn) {
            txn.exec(
                "INSERT INTO group_storage_volumes (group_id, volume_id, assigned_at) "
                "VALUES (" + txn.quote(groupId) + ", " + txn.quote(volumeId) + ", NOW())");
        });
}

void GroupQueries::removeStorageVolumeFromGroup(const unsigned int groupId, const unsigned int volumeId) {
    Transactions::exec("GroupQueries::removeStorageVolumeFromGroup",
        [&](pqxx::work& txn) {
            txn.exec("DELETE FROM group_storage_volumes WHERE group_id = " + txn.quote(groupId) +
                     " AND volume_id = " + txn.quote(volumeId));
        });
}

std::vector<std::shared_ptr<vh::types::Group>> GroupQueries::listGroupsByUser(const unsigned int userId) {
    return Transactions::exec("GroupQueries::listGroupsByUser",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<vh::types::Group>> {
            const auto res = txn.exec(
                "SELECT g.* FROM groups g "
                "JOIN group_members gm ON g.id = gm.group_id "
                "WHERE gm.user_id = " + txn.quote(userId));

            std::vector<std::shared_ptr<types::Group>> groups;
            for (const auto& group : res) {
                const auto groupId = group["id"].as<unsigned int>();

                const auto members = txn.exec(
                    "SELECT u.*, gm.joined_at "
                    "FROM users u "
                    "JOIN group_members gm ON u.id = gm.user_id "
                    "WHERE gm.group_id = " + txn.quote(groupId));

                const auto storageVolumes = txn.exec(
                "SELECT v.*, gsv.assigned_at "
                "FROM storage_volumes v "
                "JOIN group_storage_volumes gsv ON v.id = gsv.volume_id "
                "WHERE gsv.group_id = " + txn.quote(groupId));

                groups.push_back(std::make_shared<types::Group>(group, members, storageVolumes));
            }
            return groups;
        });
}

std::vector<std::shared_ptr<vh::types::Group>> GroupQueries::listGroupsByStorageVolume(const unsigned int volumeId) {
    return Transactions::exec("GroupQueries::listGroupsByStorageVolume",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<vh::types::Group>> {
            const auto res = txn.exec(
                "SELECT g.* FROM groups g "
                "JOIN group_storage_volumes gsv ON g.id = gsv.group_id "
                "WHERE gsv.volume_id = " + txn.quote(volumeId));

            std::vector<std::shared_ptr<types::Group>> groups;
            for (const auto& group : res) {
                const auto groupId = group["id"].as<unsigned int>();

                const auto members = txn.exec(
                    "SELECT u.*, gm.joined_at "
                    "FROM users u "
                    "JOIN group_members gm ON u.id = gm.user_id "
                    "WHERE gm.group_id = " + txn.quote(groupId));

                const auto storageVolumes = txn.exec(
                "SELECT v.*, gsv.assigned_at "
                "FROM storage_volumes v "
                "JOIN group_storage_volumes gsv ON v.id = gsv.volume_id "
                "WHERE gsv.group_id = " + txn.quote(groupId));

                groups.push_back(std::make_shared<types::Group>(group, members, storageVolumes));
            }
            return groups;
        });
}

std::shared_ptr<vh::types::Group> GroupQueries::getGroupByStorageVolume(const unsigned int volumeId) {
    return Transactions::exec("GroupQueries::getGroupByStorageVolume",
        [&](pqxx::work& txn) -> std::shared_ptr<vh::types::Group> {
            const auto group = txn.exec(
                "SELECT g.* FROM groups g "
                "JOIN group_storage_volumes gsv ON g.id = gsv.group_id "
                "WHERE gsv.volume_id = " + txn.quote(volumeId)).one_row();

            const auto groupId = group["id"].as<unsigned int>();

            const auto members = txn.exec(
                    "SELECT u.*, gm.joined_at "
                    "FROM users u "
                    "JOIN group_members gm ON u.id = gm.user_id "
                    "WHERE gm.group_id = " + txn.quote(groupId));

            const auto storageVolumes = txn.exec(
                "SELECT v.*, gsv.assigned_at "
                "FROM storage_volumes v "
                "JOIN group_storage_volumes gsv ON v.id = gsv.volume_id "
                "WHERE gsv.group_id = " + txn.quote(groupId));

            return std::make_shared<types::Group>(group, members, storageVolumes);
        });
}
