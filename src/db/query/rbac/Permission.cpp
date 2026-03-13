#include "db/query/rbac/Permission.hpp"
#include "db/Transactions.hpp"
#include "rbac/permission/Permission.hpp"

using namespace vh::db::model;

namespace vh::db::query::rbac {

Permission::PermPtr Permission::getPermission(const unsigned int id) {
    return Transactions::exec("Permission::getPermission", [&](pqxx::work& txn) {
        return std::make_shared<Perm>(txn.exec("SELECT * FROM permission WHERE id = " + txn.quote(id)).one_row());
    });
}

Permission::PermPtr Permission::getPermissionByName(const std::string& name) {
    return Transactions::exec("Permission::getPermissionByName", [&](pqxx::work& txn) {
        return std::make_shared<Perm>(txn.exec("SELECT * FROM permission WHERE name = " + txn.quote(name)).one_row());
    });
}

std::vector<Permission::PermPtr> Permission::listPermissions() {
    return Transactions::exec("Permission::listPermissions", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM permissions");
        std::vector<PermPtr> out;
        for (const auto& row : res) out.push_back(std::make_shared<Perm>(row));
        return out;
    });
}

unsigned short Permission::countPermissions() {
    return Transactions::exec("Permission::countPermissions", [&](pqxx::work& txn) {
        return txn.exec("SELECT COUNT(*) FROM permissions")[0][0].as<unsigned short>();
    });
}

}
