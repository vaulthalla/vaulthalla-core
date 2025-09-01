#pragma once

#include "protocols/shell/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vh::shell {

CommandResult invalid(std::string msg);
CommandResult invalid(const std::vector<std::string>& args, std::string msg);
CommandResult ok(std::string out);
CommandResult usage(const std::vector<std::string>& args);

std::optional<std::string> optVal(const CommandCall& c, const std::string& key);

std::optional<int> parseInt(const std::string& sv);

uintmax_t parseSize(const std::string& s);

[[nodiscard]] bool hasFlag(const CommandCall& c, const std::string& key);
[[nodiscard]] bool hasKey(const CommandCall& c, const std::string& key);

bool isCommandMatch(const std::vector<std::string>& path, std::string_view subcmd);

}
