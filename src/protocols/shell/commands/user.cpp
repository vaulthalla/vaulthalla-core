#include "protocols/shell/commands/user.hpp"
#include "protocols/shell/Router.hpp"
#include "database/Queries/UserQueries.hpp"
#include "util/shellArgsHelpers.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;

void vh::shell::registerUserCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("users", "list users",
        [](const CommandCall& call) -> CommandResult {
            if (!call.user->isAdmin() || !call.user->canManageUsers())
                return invalid("You do not have permission to list users.");

            return ok(to_string(UserQueries::listUsers()));
        }, {});
}
