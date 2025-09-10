#pragma once

#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"

#include <memory>
#include <string>

namespace vh::shell {
class CommandUsage;
}

namespace vh::test::cli {

struct TestUsageManager;

template <typename T = void>
struct CommandBuilder {
    explicit CommandBuilder(const std::shared_ptr<TestUsageManager>& usage, const std::string& rootTopLevelAlias);
    virtual ~CommandBuilder() = 0;

    virtual std::string create(const std::shared_ptr<T>& entity) = 0;
    virtual std::string update(const std::shared_ptr<T>& entity) = 0;
    virtual std::string remove(const std::shared_ptr<T>& entity) = 0;
    virtual std::string info(const std::shared_ptr<T>& entity) = 0;
    virtual std::string list() = 0;

    std::shared_ptr<shell::CommandUsage> root_;
};

struct UserCommandBuilder final : CommandBuilder<types::User> {
    explicit UserCommandBuilder(const std::shared_ptr<TestUsageManager>& usage);
    ~UserCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::User>& entity) override;
    std::string update(const std::shared_ptr<types::User>& entity) override;
    std::string remove(const std::shared_ptr<types::User>& entity) override;
    std::string info(const std::shared_ptr<types::User>& entity) override;
    std::string list() override;
};

struct VaultCommandBuilder final : CommandBuilder<types::Vault> {
    explicit VaultCommandBuilder(const std::shared_ptr<TestUsageManager>& usage)
        : CommandBuilder(usage, "vault") {}

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

private:
    static std::string vaultRef(const std::shared_ptr<types::Vault>& v, bool& usedName);
    static void emitOwnerIfName(std::ostringstream& oss, const std::shared_ptr<types::Vault>& v, bool usedName);

    // picks "local" or "s3";
    static std::string chooseVaultType();
};

struct GroupCommandBuilder final : CommandBuilder<types::Group> {
    explicit GroupCommandBuilder(const std::shared_ptr<TestUsageManager>& usage);
    ~GroupCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::Group>& entity) override;
    std::string update(const std::shared_ptr<types::Group>& entity) override;
    std::string remove(const std::shared_ptr<types::Group>& entity) override;
    std::string info(const std::shared_ptr<types::Group>& entity) override;
    std::string list() override;
};

struct UserRoleCommandBuilder final : CommandBuilder<types::UserRole> {
    explicit UserRoleCommandBuilder(const std::shared_ptr<TestUsageManager>& usage);
    ~UserRoleCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::UserRole>& entity) override;
    std::string update(const std::shared_ptr<types::UserRole>& entity) override;
    std::string remove(const std::shared_ptr<types::UserRole>& entity) override;
    std::string info(const std::shared_ptr<types::UserRole>& entity) override;
    std::string list() override;
};

struct VaultRoleCommandBuilder final : CommandBuilder<types::VaultRole> {
    explicit VaultRoleCommandBuilder(const std::shared_ptr<TestUsageManager>& usage);
    ~VaultRoleCommandBuilder() override = default;

    std::string create(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string update(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string remove(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string info(const std::shared_ptr<types::VaultRole>& entity) override;
    std::string list() override;
};

}
