#include "ShellUsage.hpp"
#include "SystemUsage.hpp"
#include "UserUsage.hpp"
#include "GroupUsage.hpp"
#include "RoleUsage.hpp"
#include "VaultUsage.hpp"
#include "APIKeyUsage.hpp"
#include "PermissionUsage.hpp"
#include "SecretsUsage.hpp"

using namespace vh::shell;

CommandBook ShellUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla Command Line Interface (CLI)";
    book << SystemUsage::all();
    book << UserUsage::all();
    book << GroupUsage::all();
    book << RoleUsage::all();
    book << VaultUsage::all();
    book << APIKeyUsage::all();
    book << PermissionUsage::all();
    book << SecretsUsage::all();
    return book;
}
