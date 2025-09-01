#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::secrets {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> set(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Set or update an internal secret";
    cmd->positionals = {
        {"<secret>", "Name of the secret to update (db-password | jwt-secret)"}
    };
    cmd->optional = {
        {"--file", "Path to a file containing the secret value (default=/run/vaulthalla/<secret>) (deleted after reading)"}
    };
    cmd->examples = {
        {"vh secret set db-password --file /path/to/password.txt", "Set the database password from the specified file."},
        {"vh secret set jwt-secret --file /path/to/jwt_secret.txt", "Set the JWT secret from the specified file."},
        {"vh secret set db-password", "Set the database password from the default file location. (/run/vaulthalla/db-password)"}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> secret_export(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"export", "get", "download"};
    cmd->description = "Export an internal secret to a file";
    cmd->positionals = {
        {"<secret>", "Name of the secret to export (db-password | jwt-secret | all)"}
    };
    cmd->optional = {
        {"--recipient <gpg-fingerprint>", "GPG fingerprint to encrypt the exported key (if blank will not encrypt)"},
        {"--output <file>", "Output file for the exported key (if blank will print to stdout)"}
    };
    cmd->examples = {
        {"vh secret export db-password /path/to/output_password.txt", "Export the database password to the specified file."},
        {"vh secret export jwt-secret /path/to/output_jwt_secret.txt", "Export the JWT secret to the specified file."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->description = "Manage internal secrets used by Vaulthalla.";
    cmd->subcommands = {
        set(cmd->shared_from_this()),
        secret_export(cmd->shared_from_this())
    };
    cmd->examples = {
        {"vh secret set db-password --file /path/to/password.txt", "Set the database password from the specified file."},
        {"vh secret export jwt-secret --output /path/to/output_jwt_secret.txt", "Export the JWT secret to the specified file."}
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::shared_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "Secrets Commands";
    book->root = base(parent);
    return book;
}

}
