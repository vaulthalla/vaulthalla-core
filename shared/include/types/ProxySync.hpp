#pragma once

#include "types/Sync.hpp"

namespace vh::types {

struct ProxySync : Sync {
    bool cache_thumbnails{true}, cache_full_size_objects{true};
    unsigned long long max_cache_size{0}; // in bytes

    explicit ProxySync(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const ProxySync& s);
void from_json(const nlohmann::json& j, ProxySync& s);

}
