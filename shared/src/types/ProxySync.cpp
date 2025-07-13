#include "types/ProxySync.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

ProxySync::ProxySync(const pqxx::row& row)
    : Sync(row),
      cache_thumbnails(row.at("cache_thumbnails").as<bool>()),
      cache_full_size_objects(row.at("cache_full_size_objects").as<bool>()),
      max_cache_size(row.at("max_cache_size").as<unsigned long long>()) {
}

void vh::types::to_json(nlohmann::json& j, const ProxySync& s) {
    to_json(j, static_cast<const Sync&>(s));
    j["cache_thumbnails"] = s.cache_thumbnails;
    j["cache_full_size_objects"] = s.cache_full_size_objects;
    j["max_cache_size"] = s.max_cache_size;
}

void vh::types::from_json(const nlohmann::json& j, ProxySync& s) {
    from_json(j, static_cast<Sync&>(s));
    s.cache_thumbnails = j.at("cache_thumbnails").get<bool>();
    s.cache_full_size_objects = j.at("cache_full_size_objects").get<bool>();
    s.max_cache_size = j.at("max_cache_size").get<unsigned long long>();
}
