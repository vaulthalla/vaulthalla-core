#include "SecretsUsage.hpp"

using namespace vh::shell;

CommandBook SecretsUsage::all() {
    CommandBook book;
    book.title = "Secrets Management Commands";
    book.commands = {
        secrets(),
        secrets_set(),
        secrets_export()
    };
    return book;
}

CommandUsage SecretsUsage::secrets() {
    auto cmd = buildBaseUsage_();
    cmd.description = "Manage internal secrets used by Vaulthalla.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (set, export)"}};
    cmd.examples = {
        {"vh secret set db-password --file /path/to/password.txt", "Set the database password from the specified file."},
        {"vh secret export jwt-secret --output /path/to/output_jwt_secret.txt", "Export the JWT secret to the specified file."}
    };
    return cmd;
}

CommandUsage SecretsUsage::secrets_set() {
    auto cmd = buildBaseUsage_();
    cmd.command = "set";
    cmd.command_aliases = {"update"};
    cmd.description = "Set or update an internal secret";
    cmd.positionals = {
        {"<secret>", "Name of the secret to update (db-password | jwt-secret)"}
    };
    cmd.optional = {
        {"--file", "Path to a file containing the secret value (default=/run/vaulthalla/<secret>) (deleted after reading)"}
    };
    cmd.examples = {
        {"vh secret set db-password --file /path/to/password.txt", "Set the database password from the specified file."},
        {"vh secret set jwt-secret --file /path/to/jwt_secret.txt", "Set the JWT secret from the specified file."},
        {"vh secret set db-password", "Set the database password from the default file location. (/run/vaulthalla/db-password)"}
    };
    return cmd;
}

CommandUsage SecretsUsage::secrets_export() {
    auto cmd = buildBaseUsage_();
    cmd.command = "export";
    cmd.command_aliases = {"get", "show"};
    cmd.description = "Export an internal secret to a file";
    cmd.positionals = {
        {"<secret>", "Name of the secret to export (db-password | jwt-secret | all)"}
    };
    cmd.optional = {
        {"--recipient <gpg-fingerprint>", "GPG fingerprint to encrypt the exported key (if blank will not encrypt)"},
        {"--output <file>", "Output file for the exported key (if blank will print to stdout)"}
    };
    cmd.examples = {
        {"vh secret export db-password /path/to/output_password.txt", "Export the database password to the specified file."},
        {"vh secret export jwt-secret /path/to/output_jwt_secret.txt", "Export the JWT secret to the specified file."}
    };
    return cmd;
}

CommandUsage SecretsUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "secrets";
    cmd.ns_aliases = {"secret"};
    return cmd;
}
