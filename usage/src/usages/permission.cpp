#include "usages.hpp"

#include <sstream>

using namespace vh::shell;

namespace vh::shell::permissions {

std::string usage_user_permissions() {
    std::ostringstream os;
    os << "Permission Flags:\n"
       << "  --manage-encryption-keys   | --allow-manage-encryption-keys  | --deny-manage-encryption-keys\n"
       << "  --manage-admins            | --allow-manage-admins           | --deny-manage-admins\n"
       << "  --manage-users             | --allow-manage-users            | --deny-manage-users\n"
       << "  --manage-groups            | --allow-manage-groups           | --deny-manage-groups\n"
       << "  --manage-vaults            | --allow-manage-vaults           | --deny-manage-vaults\n"
       << "  --manage-groups            | --allow-manage-groups           | --deny-manage-groups\n"
       << "  --manage-roles             | --allow-manage-roles            | --deny-manage-roles\n"
       << "  --manage-api-keys          | --allow-manage-api-keys         | --deny-manage-api-keys\n"
       << "  --audit-log-access         | --allow-audit-log-access        | --deny-audit-log-access\n"
       << "  --create-vaults            | --allow-create-vaults           | --deny-create-vaults\n"
       << "\n"
       << "You can use either the --manage-* shorthand to set, or explicitly use --set/--unset.\n";
    return os.str();
}

std::string usage_vault_permissions() {
    std::ostringstream os;
    os << "Vault Permissions Flags:\n"
       << "  --manage-vault        | --allow-manage-vault        | --deny-manage-vault\n"
       << "  --manage-access       | --allow-manage-access       | --deny-manage-access\n"
       << "  --manage-tags         | --allow-manage-tags         | --deny-manage-tags\n"
       << "  --manage-metadata     | --allow-manage-metadata     | --deny-manage-metadata\n"
       << "  --manage-versions     | --allow-manage-versions     | --deny-manage-versions\n"
       << "  --manage-file-locks   | --allow-manage-file-locks   | --deny-manage-file-locks\n"
       << "  --share               | --allow-share               | --deny-share\n"
       << "  --sync                | --allow-sync                | --deny-sync\n"
       << "  --create              | --allow-create              | --deny-create\n"
       << "  --download            | --allow-download            | --deny-download\n"
       << "  --delete              | --allow-delete              | --deny-delete\n"
       << "  --rename              | --allow-rename              | --deny-rename\n"
       << "  --move                | --allow-move                | --deny-move\n"
       << "  --list                | --allow-list                | --deny-list\n"
       << "\n"
       << "Use --allow-* or --deny-* to modify permissions individually,\n"
       << "or use the shorthand (e.g. --share) to enable directly.\n";
    return os.str();
}

static std::shared_ptr<CommandUsage> base(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    cmd->aliases = {"permission", "permissions", "perm", "perms"};
    cmd->optional = {
        Optional::OneToMany(
            "type_filter",
            "Filter permissions by type",
            "type",
            {"user", "vault"},
            "both"
            )
    };
    cmd->optional_flags = {
        {"user_filter", "Filter permissions for user roles", {"user", "u"}},
        {"vault_filter", "Filter permissions for vault roles", {"vault", "v"}},
        {"help", "Show help message", {"h"}}
    };
    cmd->description = "Display available permission flags for user and vault roles.";
    cmd->examples.push_back({"vh permissions", "Show all available permission flags."});
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = base(parent);
    const auto book = std::make_shared<CommandBook>();
    book->title = "Permission Commands";
    book->root = cmd;
    return book;
}

}
