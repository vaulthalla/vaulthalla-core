#pragma once

#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "updateAliases.hpp"

#include <memory>
#include <string>

namespace vh::shell {
class CommandUsage;
class UsageManager;
}

namespace vh::test::cli {

template <typename T = void>
class CommandBuilder {
public:
    virtual ~CommandBuilder() = default;
    explicit CommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx, const std::string& rootTopLevelAlias)
        : ctx_(ctx) {
        if (!usage) throw std::runtime_error("CommandBuilder: usage manager is null");
        const auto cmd = usage->resolve(rootTopLevelAlias);
        if (!cmd) throw std::runtime_error("CommandBuilder: command usage not found for root: " + rootTopLevelAlias);
        root_ = cmd;
    }

    virtual std::string create(const std::shared_ptr<T>& entity) = 0;
    virtual std::string update(const std::shared_ptr<T>& entity) = 0;
    virtual std::string remove(const std::shared_ptr<T>& entity) = 0;
    virtual std::string info(const std::shared_ptr<T>& entity) = 0;
    virtual std::string list() = 0;

protected:
    std::shared_ptr<shell::CommandUsage> root_;
    std::shared_ptr<CLITestContext> ctx_;

    virtual std::string updateAndResolveVar(const std::shared_ptr<T>& entity, const std::string& field) = 0;
};

class UserCommandBuilder final : CommandBuilder<types::User> {
public:
    explicit UserCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx);
    ~UserCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::User>& entity) override;
    std::string update(const std::shared_ptr<types::User>& entity) override;
    std::string remove(const std::shared_ptr<types::User>& entity) override;
    std::string info(const std::shared_ptr<types::User>& entity) override;
    std::string list() override;

protected:
    std::string updateAndResolveVar(const std::shared_ptr<types::User>& entity, const std::string& field) override;

private:
    UserAliases userAliases_;
};

class VaultCommandBuilder final : CommandBuilder<types::Vault> {
public:
    explicit VaultCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx);
    ~VaultCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::Vault>& v) override;
    std::string update(const std::shared_ptr<types::Vault>& v) override;
    std::string remove(const std::shared_ptr<types::Vault>& v) override;
    std::string info  (const std::shared_ptr<types::Vault>& v) override;
    std::string list() override;

    std::string sync_set   (const std::shared_ptr<types::Vault>& v);
    std::string sync_info  (const std::shared_ptr<types::Vault>& v);
    std::string sync_trigger(const std::shared_ptr<types::Vault>& v);

    std::string key_export(const std::shared_ptr<types::Vault>& v);
    std::string key_rotate(const std::shared_ptr<types::Vault>& v);

protected:
    std::string updateAndResolveVar(const std::shared_ptr<types::Vault>& entity, const std::string& field) override;

private:
    S3VaultAliases vaultAliases_;

    static std::string vaultRef(const std::shared_ptr<types::Vault>& v, bool& usedName);
    static void emitOwnerIfName(std::ostringstream& oss, const std::shared_ptr<types::Vault>& v, bool usedName);

    // picks "local" or "s3";
    static std::string chooseVaultType();
};

class GroupCommandBuilder final : CommandBuilder<types::Group> {
public:
    explicit GroupCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx);
    ~GroupCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::Group>& entity) override;
    std::string update(const std::shared_ptr<types::Group>& entity) override;
    std::string remove(const std::shared_ptr<types::Group>& entity) override;
    std::string info(const std::shared_ptr<types::Group>& entity) override;
    std::string list() override;

protected:
    std::string updateAndResolveVar(const std::shared_ptr<types::Group>& entity, const std::string& field) override;

private:
    GroupAliases groupAliases_;
};

class UserRoleCommandBuilder final : CommandBuilder<types::UserRole> {
public:

    explicit UserRoleCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx);
    ~UserRoleCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::UserRole>& entity) override;
    std::string update(const std::shared_ptr<types::UserRole>& entity) override;
    std::string remove(const std::shared_ptr<types::UserRole>& entity) override;
    std::string info(const std::shared_ptr<types::UserRole>& entity) override;
    std::string list() override;

protected:
    std::string updateAndResolveVar(const std::shared_ptr<types::UserRole>& entity, const std::string& field) override;

private:
    UserRoleAliases userRoleAliases_;
};

class VaultRoleCommandBuilder final : CommandBuilder<types::VaultRole> {
public:
    explicit VaultRoleCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx);
    ~VaultRoleCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string update(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string remove(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string info(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string list() override;

protected:
    std::string updateAndResolveVar(const std::shared_ptr<types::VaultRole>& entity, const std::string& field) override;

private:
    VaultRoleAliases vaultRoleAliases_;
};

}
