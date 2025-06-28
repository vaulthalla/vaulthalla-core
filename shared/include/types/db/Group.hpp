#pragma once

#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct User;
struct Volume;

struct GroupMember {
    std::shared_ptr<User> user;
    std::time_t joined_at;

    GroupMember() = default;
    explicit GroupMember(const pqxx::row& row);
};

struct GroupStorageVolume {
    std::shared_ptr<Volume> volume;
    std::time_t assigned_at;

    GroupStorageVolume() = default;
    explicit GroupStorageVolume(const pqxx::row& row);
};

struct Group {
    unsigned int id, gid{};
    std::string name;
    std::optional<std::string> description;
    std::time_t created_at;
    std::optional<std::time_t> updated_at;
    std::vector<std::shared_ptr<GroupMember>> members;
    std::vector<std::shared_ptr<GroupStorageVolume>> volumes;

    Group() = default;
    explicit Group(const pqxx::row& gr, const pqxx::result& members, const pqxx::result& storageVolumes);
    explicit Group(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const Group& g);
void from_json(const nlohmann::json& j, Group& g);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Group>>& groups);
std::vector<std::shared_ptr<Group>> groups_from_json(const nlohmann::json& j);

void to_json(nlohmann::json& j, const GroupMember& gm);
void to_json(nlohmann::json& j, const GroupStorageVolume& gsv);

} // namespace vh::types
