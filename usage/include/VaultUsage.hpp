#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

class VaultUsage {
public:
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage vaults_list();
    [[nodiscard]] static CommandUsage vault();
    [[nodiscard]] static CommandUsage vault_create();
    [[nodiscard]] static CommandUsage vault_delete();
    [[nodiscard]] static CommandUsage vault_info();
    [[nodiscard]] static CommandUsage vault_update();
    [[nodiscard]] static CommandUsage vault_role_assign();
    [[nodiscard]] static CommandUsage vault_role_remove_assignment();
    [[nodiscard]] static CommandUsage vault_role_add_override();
    [[nodiscard]] static CommandUsage vault_role_remove_override();
    [[nodiscard]] static CommandUsage vault_role_update_override();
    [[nodiscard]] static CommandUsage vault_role_override();
    [[nodiscard]] static CommandUsage vault_role_remove();
    [[nodiscard]] static CommandUsage vault_role_list();
    [[nodiscard]] static CommandUsage vault_role();
    [[nodiscard]] static CommandUsage vault_keys();
    [[nodiscard]] static CommandUsage vault_sync();

private:
    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
