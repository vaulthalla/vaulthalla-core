#include "protocols/shell/usage/SystemUsage.hpp"

using namespace vh::shell;

CommandBook SystemUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla System Commands";
    book.commands = {
        help(),
        version()
    };
    return book;
}

CommandUsage SystemUsage::system() {
    auto cmd = buildBaseUsage_();
    cmd.description = "General system commands.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (help, version)"}};
    cmd.examples.push_back({"vh help", "Show general help information."});
    cmd.examples.push_back({"vh version", "Show the current version of Vaulthalla."});
    return cmd;
}

CommandUsage SystemUsage::help() {
    CommandUsage cmd = buildBaseUsage_();
    cmd.command = "help";
    cmd.command_aliases = {"h", "?", "--help", "-h"};
    cmd.description = "Show help information about commands.";
    cmd.optional = {
        {"<command>", "Optional command name to get detailed help"}
    };
    cmd.examples.push_back({"vh help", "Show general help information."});
    cmd.examples.push_back({"vh help api-keys", "Show detailed help for the 'api-keys' command."});
    return cmd;
}

CommandUsage SystemUsage::version() {
    CommandUsage cmd = buildBaseUsage_();
    cmd.command = "version";
    cmd.command_aliases = {"v", "--version", "-v"};
    cmd.description = "Show version information about Vaulthalla.";
    cmd.examples.push_back({"vh version", "Show the current version of Vaulthalla."});
    return cmd;
}

CommandUsage SystemUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "system";
    cmd.ns_aliases = {"sys"};
    return cmd;
}
