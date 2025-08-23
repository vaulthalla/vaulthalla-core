#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

class APIKeyUsage {
public:
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage apikeys_list();
    [[nodiscard]] static CommandUsage apikey();
    [[nodiscard]] static CommandUsage apikey_create();
    [[nodiscard]] static CommandUsage apikey_delete();
    [[nodiscard]] static CommandUsage apikey_info();
    [[nodiscard]] static CommandUsage apikey_update();

    [[nodiscard]] static std::string usage_provider();

private:
    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
