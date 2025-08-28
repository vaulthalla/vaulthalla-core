#pragma once

#include "protocols/shell/types.hpp"

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>

namespace vh::shell {

class SocketIO final : public IO {
public:
    explicit SocketIO(int fd);
    void print(std::string_view msg) override;
    bool confirm(std::string_view prompt, bool def_no = true) override;
    std::string prompt(std::string_view prompt,
                       std::string_view def = "") override;

    static nlohmann::json recv_json(int fd);
    static void send_json(int fd, const nlohmann::json& j);

private:
    int fd_, id_counter_ = 0;
    std::string next_id();
};

}
