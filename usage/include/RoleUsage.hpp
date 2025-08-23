#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

class RoleUsage {
public:
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage roles_list();
    [[nodiscard]] static CommandUsage role();
    [[nodiscard]] static CommandUsage role_create();
    [[nodiscard]] static CommandUsage role_delete();
    [[nodiscard]] static CommandUsage role_info();
    [[nodiscard]] static CommandUsage role_update();

private:
    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
