#include "protocols/shell/usage/PermissionUsage.hpp"

#include <sstream>

using namespace vh::shell;

std::string PermissionUsage::usage_user_permissions() {
    std::ostringstream os;
    os << "Permission Flags:\n"
       << "  --manage-encryption-keys   | --set-manage-encryption-keys  | --unset-manage-encryption-keys\n"
       << "  --manage-admins            | --set-manage-admins           | --unset-manage-admins\n"
       << "  --manage-users             | --set-manage-users            | --unset-manage-users\n"
       << "  --manage-groups            | --set-manage-groups           | --unset-manage-groups\n"
       << "  --manage-vaults            | --set-manage-vaults           | --unset-manage-vaults\n"
       << "  --manage-groups            | --set-manage-groups           | --unset-manage-groups\n"
       << "  --manage-roles             | --set-manage-roles            | --unset-manage-roles\n"
       << "  --manage-api-keys          | --set-manage-api-keys         | --unset-manage-api-keys\n"
       << "  --audit-log-access         | --set-audit-log-access        | --unset-audit-log-access\n"
       << "  --create-vaults            | --set-create-vaults           | --unset-create-vaults\n"
       << "\n"
       << "You can use either the --manage-* shorthand to set, or explicitly use --set/--unset.\n";
    return os.str();
}

std::string PermissionUsage::usage_vault_permissions() {
    std::ostringstream os;
    os << "Vault Permissions Flags:\n"
       << "  --migrate-data        | --set-migrate-data        | --unset-migrate-data\n"
       << "  --manage-access       | --set-manage-access       | --unset-manage-access\n"
       << "  --manage-tags         | --set-manage-tags         | --unset-manage-tags\n"
       << "  --manage-metadata     | --set-manage-metadata     | --unset-manage-metadata\n"
       << "  --manage-versions     | --set-manage-versions     | --unset-manage-versions\n"
       << "  --manage-file-locks   | --set-manage-file-locks   | --unset-manage-file-locks\n"
       << "  --share               | --set-share               | --unset-share\n"
       << "  --sync                | --set-sync                | --unset-sync\n"
       << "  --create              | --set-create              | --unset-create\n"
       << "  --download            | --set-download            | --unset-download\n"
       << "  --delete              | --set-delete              | --unset-delete\n"
       << "  --rename              | --set-rename              | --unset-rename\n"
       << "  --move                | --set-move                | --unset-move\n"
       << "  --list                | --set-list                | --unset-list\n"
       << "\n"
       << "Use --set-* or --unset-* to modify permissions individually,\n"
       << "or use the shorthand (e.g. --share) to enable directly.\n";
    return os.str();
}

CommandBook PermissionUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla Permission Commands";
    book.commands = {permissions()};
    return book;
}


CommandUsage PermissionUsage::permissions() {
    CommandUsage cmd;
    cmd.ns = "permissions";
    cmd.ns_aliases = {"permission", "p"};
    cmd.optional = {{"<type>", "Type of permissions to display: 'user'/'u' or 'vault'/'v' (default: both)"}};
    cmd.description = "Display available permission flags for user and vault roles.";
    cmd.examples.push_back({"vh permissions", "Show all available permission flags."});
    return cmd;
}
