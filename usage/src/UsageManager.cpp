#include "UsageManager.hpp"
#include "CommandBook.hpp"
#include "CommandUsage.hpp"
#include "usages.hpp"

#include <sstream>

using namespace vh::shell;

UsageManager::UsageManager() {
    root_ = std::make_shared<CommandUsage>();
    root_->aliases = {"vh"};
    root_->description = "Vaulthalla Command Line Interface";
    root_->optional = {
        {"-h | --help", "Show help information"},
        {"-v | --version", "Show version information"}
    };
    root_->examples = {
        {"vh help", "Show general help information."},
        {"vh version", "Show the current version of Vaulthalla."}
    };

    registerBook(vault::get(root_));
    registerBook(user::get(root_));
    registerBook(group::get(root_));
    registerBook(secrets::get(root_));
    registerBook(aku::get(root_));
    registerBook(role::get(root_));
    registerBook(permissions::get(root_));
    registerBook(help::get(root_));
    registerBook(version::get(root_));
}

void UsageManager::registerBook(const std::shared_ptr<CommandBook>& book) {
    root_->subcommands.push_back(book->root);
    for (const auto& alias : book->root->aliases) {
        if (index_.contains(alias)) throw std::runtime_error("UsageManager: duplicate top-level alias: " + alias);
        index_[alias] = book;
    }
}

std::shared_ptr<CommandBook> UsageManager::bookFor(const std::string& topLevelAlias) const {
    if (topLevelAlias.empty()) return nullptr;
    if (index_.contains(topLevelAlias)) return index_.at(topLevelAlias);
    return nullptr;
}

std::shared_ptr<CommandUsage> UsageManager::resolve(const std::vector<std::string>& args) const {
    if (args.empty()) return nullptr;
    if (args[0] != "vh") throw std::runtime_error("UsageManager: resolve called with non-vh root");
    if (args.size() == 1) return root_;
    const auto book = bookFor(args[1]);
    if (!book) return nullptr;
    if (args.size() == 2) return book->root;
    return book->resolve(args);
}

std::string UsageManager::renderHelp(const std::vector<std::string>& args) const {
    if (args.size() < 2) return root_->basicStr();
    const auto book = bookFor(args[0]);
    if (!book) {
        std::ostringstream out;
        out << "Unknown command or alias: ";
        for (const auto& a : args) out << a << ' ';
        out << "\n\n";
        out << index_.begin()->second->root->basicStr(true);
        return out.str();
    }
    if (args.size() == 1) return book->renderHelp({"vh"});
    return book->renderHelp(args);
}

