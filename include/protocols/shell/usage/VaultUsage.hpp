#pragma once

#include "protocols/shell/CommandUsage.hpp"

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
    [[nodiscard]] static CommandUsage vault_role();
    [[nodiscard]] static CommandUsage vault_keys();
    [[nodiscard]] static CommandUsage vault_sync();

private:
    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
