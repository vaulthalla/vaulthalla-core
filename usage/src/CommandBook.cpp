#include "CommandBook.hpp"
#include "CommandUsage.hpp"

#include <sstream>
#include <algorithm>
#include <ranges>

using namespace vh::protocols::shell;

std::string CommandBook::str() const {
    std::ostringstream out;
    if (!title.empty()) out << title << "\n\n";
    out << root->str();
    return out.str();
}

std::string CommandBook::basicStr() const {
    std::ostringstream out;
    if (!title.empty()) out << title << "\n\n";
    out << root->basicStr();
    return out.str();
}

std::string CommandBook::markdown() const {
    std::ostringstream md;
    if (!title.empty()) md << "# " << title << "\n\n";
    md << root->markdown();
    return md.str();
}

std::shared_ptr<CommandUsage> CommandBook::resolve(const std::vector<std::string>& args) const {
    if (!root) throw std::runtime_error("CommandBook::resolve() called with no root");
    auto current = root;
    for (const auto& arg : args) {
        auto it = std::ranges::find_if(current->subcommands.begin(), current->subcommands.end(),
                               [&](const std::shared_ptr<CommandUsage>& cu) {
                                   return cu->matches(arg);
                               });
        if (it == current->subcommands.end()) return current;
        current = *it;
    }
    return current;
}

std::string CommandBook::renderHelp(const std::vector<std::string>& args) const {
    const auto cmd = resolve(args);
    std::ostringstream out;
    if (!cmd) {
        out << root->basicStr();
        out << "\n\n";
        out << "[CommandBook] Unknown command or alias: ";
        for (const auto& a : args) out << a << ' ';
        out << "root is at: " << root->primary() << "\n";
        return out.str();
    }
    out << "\n" << book_theme->H() << title << book_theme->R() << "\n\n";
    return out.str() + cmd->str();
}
