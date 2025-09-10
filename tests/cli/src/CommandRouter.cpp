#include "CommandRouter.hpp"
#include "TestCase.hpp"
#include "EntityRegistrar.hpp"
#include "ListInfoHandler.hpp"
#include "UpdateHandler.hpp"
#include "CLITestContext.hpp"
#include "TestUsageManager.hpp"
#include "protocols/shell/Router.hpp"

#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"

using namespace vh::test::cli;
using namespace vh::shell;
using namespace vh::types;

CommandRouter::CommandRouter(const std::shared_ptr<CLITestContext>& ctx, const std::shared_ptr<TestUsageManager>& usage) {
    const auto router = std::make_shared<Router>();
    registrar_ = std::make_shared<EntityRegistrar>(router, ctx);
    list_info_handler_ = std::make_shared<ListInfoHandler>(ctx, router);
    update_handler_ = std::make_shared<UpdateHandler>(usage, router, ctx);
    registerAll();
}

void CommandRouter::registerRoute(const std::string& path, const std::function<EntityResult(const std::shared_ptr<void>& entity)>& handler) {
    if (routes_.contains(path)) throw std::runtime_error("CommandRouter: route already registered for path: " + path);
    routes_[path] = handler;
}

std::shared_ptr<TestCase> CommandRouter::route(const std::shared_ptr<TestCase>& test) const {
    if (!routes_.contains(test->path)) throw std::runtime_error("CommandRouter: no route registered for path: " + test->path);
    const auto [result, entity] = routes_.at(test->path)(test->entity);
    test->result = result;
    test->entity = entity;
    return test;
}

std::vector<std::shared_ptr<TestCase>> CommandRouter::route(const std::vector<std::shared_ptr<TestCase>>& tests) const {
    std::vector<std::shared_ptr<TestCase>> results;

    for (auto& t : tests) {
        if (!routes_.contains(t->path)) throw std::runtime_error("CommandRouter: no route registered for path: " + t->path);
        const auto [result, entity] = routes_.at(t->path)(t->entity);
        t->result = result;
        t->entity = entity;
        results.push_back(t);
    }

    return results;
}

void CommandRouter::registerAll() {
    registerRoute("user/create", [&](const std::shared_ptr<void>&) -> EntityResult {
        return registrar_->create(EntityType::USER);
    });

    registerRoute("group/create", [&](const std::shared_ptr<void>&) -> EntityResult {
        return registrar_->create(EntityType::GROUP);
    });

    registerRoute("vault/create", [&](const std::shared_ptr<void>&) -> EntityResult {
        return registrar_->create(EntityType::VAULT);
    });

    registerRoute("role/create/user", [&](const std::shared_ptr<void>&) -> EntityResult {
        return registrar_->create(EntityType::USER_ROLE);
    });

    registerRoute("role/create/vault", [&](const std::shared_ptr<void>&) -> EntityResult {
        return registrar_->create(EntityType::VAULT_ROLE);
    });

    registerRoute("user/update", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no user entity provided for update");
        const auto user = std::static_pointer_cast<User>(entity);
        return update_handler_->update(EntityType::USER, user);
    });

    registerRoute("vault/update", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no vault entity provided for update");
        const auto vault = std::static_pointer_cast<Vault>(entity);
        return update_handler_->update(EntityType::VAULT, vault);
    });

    registerRoute("group/update", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no group entity provided for update");
        const auto group = std::static_pointer_cast<Group>(entity);
        return update_handler_->update(EntityType::GROUP, group);
    });

    registerRoute("role/update/user", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no user role entity provided for update");
        const auto role = std::static_pointer_cast<UserRole>(entity);
        return update_handler_->update(EntityType::USER_ROLE, role);
    });

    registerRoute("role/update/vault", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no vault role entity provided for update");
        const auto role = std::static_pointer_cast<VaultRole>(entity);
        return update_handler_->update(EntityType::VAULT_ROLE, role);
    });

    registerRoute("user/list", [&](const std::shared_ptr<void>&) -> EntityResult {
        return list_info_handler_->list<User>(EntityType::USER);
    });

    registerRoute("group/list", [&](const std::shared_ptr<void>&) -> EntityResult {
        return list_info_handler_->list<User>(EntityType::GROUP);
    });

    registerRoute("vault/list", [&](const std::shared_ptr<void>&) -> EntityResult {
        return list_info_handler_->list<User>(EntityType::VAULT);
    });

    registerRoute("role/list/user", [&](const std::shared_ptr<void>&) -> EntityResult {
        return list_info_handler_->list<User>(EntityType::USER_ROLE);
    });

    registerRoute("role/list/vault", [&](const std::shared_ptr<void>&) -> EntityResult {
        return list_info_handler_->list<User>(EntityType::VAULT_ROLE);
    });

    registerRoute("user/info", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no user entity provided for info");
        const auto user = std::static_pointer_cast<User>(entity);
        return list_info_handler_->info(EntityType::USER, user);
    });

    registerRoute("group/info", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no group entity provided for info");
        const auto group = std::static_pointer_cast<Group>(entity);
        return list_info_handler_->info(EntityType::GROUP, group);
    });

    registerRoute("vault/info", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no vault entity provided for info");
        const auto vault = std::static_pointer_cast<Vault>(entity);
        return list_info_handler_->info(EntityType::VAULT, vault);
    });

    registerRoute("role/info/user", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no user role entity provided for info");
        const auto role = std::static_pointer_cast<UserRole>(entity);
        return list_info_handler_->info(EntityType::USER_ROLE, role);
    });

    registerRoute("role/info/vault", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no vault role entity provided for info");
        const auto role = std::static_pointer_cast<VaultRole>(entity);
        return list_info_handler_->info(EntityType::VAULT_ROLE, role);
    });

    registerRoute("user/delete", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no user entity provided for deletion");
        const auto user = std::static_pointer_cast<User>(entity);
        return registrar_->remove(EntityType::USER, user);
    });

    registerRoute("group/delete", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no group entity provided for deletion");
        const auto group = std::static_pointer_cast<Group>(entity);
        return registrar_->remove(EntityType::GROUP, group);
    });

    registerRoute("vault/delete", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no vault entity provided for deletion");
        const auto vault = std::static_pointer_cast<Vault>(entity);
        return registrar_->remove(EntityType::VAULT, vault);
    });

    registerRoute("role/delete/user", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no user role entity provided for deletion");
        const auto role = std::static_pointer_cast<UserRole>(entity);
        return registrar_->remove(EntityType::USER_ROLE, role);
    });

    registerRoute("role/delete/vault", [&](const std::shared_ptr<void>& entity) -> EntityResult {
        if (!entity) throw std::runtime_error("CommandRouter: no vault role entity provided for deletion");
        const auto role = std::static_pointer_cast<VaultRole>(entity);
        return registrar_->remove(EntityType::VAULT_ROLE, role);
    });
}

