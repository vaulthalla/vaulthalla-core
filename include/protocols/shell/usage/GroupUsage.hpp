#pragma once

#include "protocols/shell/CommandUsage.hpp"

namespace vh::shell {

class GroupUsage {
public:
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage groups_list();
    [[nodiscard]] static CommandUsage group();
    [[nodiscard]] static CommandUsage group_create();
    [[nodiscard]] static CommandUsage group_delete();
    [[nodiscard]] static CommandUsage group_info();
    [[nodiscard]] static CommandUsage group_update();
    [[nodiscard]] static CommandUsage group_user();
    [[nodiscard]] static CommandUsage group_list_users();

private:
    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
