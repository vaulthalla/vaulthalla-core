#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <cstdint>

namespace pqxx {
    class row;
}

namespace vh::crypto::model {

struct InternalSecret {
    std::string key;
    std::vector<uint8_t> value, iv;
    std::time_t created_at{std::time(nullptr)}, updated_at{std::time(nullptr)};

    InternalSecret() = default;
    explicit InternalSecret(const pqxx::row& row);
};

}
