#include "protocols/websocket/handlers/GroupHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "types/User.hpp"
#include "types/Group.hpp"

#include <nlohmann/json.hpp>

#include "auth/AuthManager.hpp"
#include "database/Queries/UserQueries.hpp"

using namespace vh::websocket;
using namespace vh::database;

json GroupHandler::add(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can create groups");

    const std::string groupName = payload.at("name").get<std::string>();
    const std::string groupDescription = payload.value("description", "");

    const auto group = std::make_shared<types::Group>();
    group->name = groupName;
    group->description = groupDescription;
    if (payload.contains("linux_gid")) {
        const auto linuxGid = payload.at("linux_gid").get<unsigned int>();
        if (linuxGid <= 0)
            throw std::runtime_error("Invalid Linux GID: must be a positive integer");
        group->linux_gid = linuxGid;
    }

    GroupQueries::createGroup(group);

    return {{"name", groupName}};
}

json GroupHandler::remove(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can delete groups");

    const auto groupId = payload.at("id").get<unsigned int>();
    GroupQueries::deleteGroup(groupId);

    return {{"id", groupId}};
}

json GroupHandler::addMember(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can add members to groups");

    const unsigned int groupId = payload.at("groupId").get<unsigned int>();
    const std::string memberName = payload.at("memberName").get<std::string>();

    const auto member = UserQueries::getUserByName(memberName);
    if (!member) throw std::runtime_error("User not found: " + memberName);

    GroupQueries::addMemberToGroup(groupId, member->id);

    return {{"groupId", groupId}, {"memberName", memberName}};
}

json GroupHandler::removeMember(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can remove members from groups");

    const unsigned int groupId = payload.at("groupId").get<unsigned int>();
    const unsigned int userId = payload.at("userId").get<unsigned int>();

    GroupQueries::removeMemberFromGroup(groupId, userId);

    return {{"groupId", groupId}, {"userId", userId}};
}

json GroupHandler::list(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list groups");

    return {{"groups", GroupQueries::listGroups()}};
}

json GroupHandler::get(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get group details");

    const auto groupId = payload.at("id").get<unsigned int>();
    auto group = GroupQueries::getGroup(groupId);

    if (!group) throw std::runtime_error("Group not found");

    return {{"group", *group}};
}

json GroupHandler::getByName(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get group by name");

    if (const auto group = GroupQueries::getGroupByName(payload.at("name").get<std::string>()))
        return {{"group", *group}};

    throw std::runtime_error("Group not found");
}


json GroupHandler::update(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can update groups");

    const unsigned int groupId = payload.at("id").get<unsigned int>();
    const std::string newName = payload.at("name").get<std::string>();

    if (!auth::AuthManager::isValidGroup(newName))
        throw std::runtime_error("Invalid group name: " + newName);

    const auto group = GroupQueries::getGroup(groupId);
    if (!group) throw std::runtime_error("Group not found");

    group->name = newName;

    if (payload.contains("linux_gid")) {
        const auto linux_gid = payload.at("linux_gid").get<unsigned int>();
        if (linux_gid <= 0)
            throw std::runtime_error("Invalid Linux GID: must be a positive integer");
        group->linux_gid = linux_gid;
    }

    GroupQueries::updateGroup(group);

    return {{"id", groupId}, {"name", newName}};
}

json GroupHandler::listByUser(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list groups by user");

    return {{"groups", GroupQueries::listGroups(payload.at("user_id").get<unsigned int>())}};
}
