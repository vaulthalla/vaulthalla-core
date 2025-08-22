#include "protocols/shell/usage/ShellUsage.hpp"
#include "protocols/shell/usage/SystemUsage.hpp"
#include "protocols/shell/usage/UserUsage.hpp"
#include "protocols/shell/usage/GroupUsage.hpp"
#include "protocols/shell/usage/RoleUsage.hpp"
#include "protocols/shell/usage/VaultUsage.hpp"

using namespace vh::shell;

CommandBook ShellUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla Command Line Interface (CLI)";
    book << SystemUsage::all();
    book << UserUsage::all();
    book << GroupUsage::all();
    book << RoleUsage::all();
    book << VaultUsage::all();
    return book;
}
