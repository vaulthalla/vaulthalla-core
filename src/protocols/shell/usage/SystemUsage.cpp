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

CommandUsage SystemUsage::help() {
    CommandUsage cmd;
    cmd.ns = "help";
    cmd.ns_aliases = {"h", "?", "--help", "-h"};
    cmd.description = "Explicitly show help about a command or namespace (optional).";
    cmd.optional = {
        {"<command>", "Optional command name to get detailed help"}
    };
    cmd.examples = {
        {"vh help", "Show general help information."},
        {"vh help api-keys", "Show detailed help for the 'api-keys' command."},
        {"vh vault", "Call a command namespace with no args to show its help."}
    };
    return cmd;
}

CommandUsage SystemUsage::version() {
    CommandUsage cmd;
    cmd.ns = "version";
    cmd.ns_aliases = {"-v", "--v", "--version"};
    cmd.description = "Show version information about Vaulthalla.";
    cmd.examples.push_back({"vh version", "Show the current version of Vaulthalla."});
    return cmd;
}
