#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

class UserUsage {
public:
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage users_list();
    [[nodiscard]] static CommandUsage user();
    [[nodiscard]] static CommandUsage user_create();
    [[nodiscard]] static CommandUsage user_delete();
    [[nodiscard]] static CommandUsage user_info();
    [[nodiscard]] static CommandUsage user_update();

private:
    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
