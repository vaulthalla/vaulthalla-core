#include "protocols/websocket/handlers/GroupHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "types/User.hpp"
#include "types/Group.hpp"

#include <nlohmann/json.hpp>

#include "auth/AuthManager.hpp"
#include "database/Queries/UserQueries.hpp"

using namespace vh::websocket;

void GroupHandler::handleCreateGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can create groups");

        const auto& payload = msg.at("payload");
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

        database::GroupQueries::createGroup(group);

        const json response = {
            {"command", "group.add.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", {{"name", groupName}}}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.add.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleDeleteGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can delete groups");

        const auto groupId = msg.at("payload").at("id").get<unsigned int>();
        database::GroupQueries::deleteGroup(groupId);

        const json response = {
            {"command", "group.remove.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.remove.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleAddMemberToGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can add members to groups");

        const auto& payload = msg.at("payload");
        const unsigned int groupId = payload.at("groupId").get<unsigned int>();
        const std::string memberName = payload.at("memberName").get<std::string>();

        const auto member = database::UserQueries::getUserByName(memberName);
        if (!member) throw std::runtime_error("User not found: " + memberName);

        database::GroupQueries::addMemberToGroup(groupId, member->id);

        const json response = {
            {"command", "group.member.add.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", {{"groupId", groupId}, {"memberName", memberName}}}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.member.add.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleRemoveMemberFromGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can remove members from groups");

        const auto& payload = msg.at("payload");
        const unsigned int groupId = payload.at("groupId").get<unsigned int>();
        const unsigned int userId = payload.at("userId").get<unsigned int>();

        database::GroupQueries::removeMemberFromGroup(groupId, userId);

        const json response = {
            {"command", "group.member.remove.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", {{"groupId", groupId}, {"userId", userId}}}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.member.remove.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleListGroups(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can list groups");

        const auto groups = database::GroupQueries::listGroups();

        const json data = {{"groups", groups}};

        const json response = {
            {"command", "groups.list.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "groups.list.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleGetGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can get group details");

        const auto groupId = msg.at("payload").at("id").get<unsigned int>();
        auto group = database::GroupQueries::getGroup(groupId);

        if (!group) throw std::runtime_error("Group not found");

        const json data = {{"group", *group}};

        const json response = {
            {"command", "group.get.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.get.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleGetGroupByName(const ::json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can get group by name");

        const auto groupName = msg.at("payload").at("name").get<std::string>();
        auto group = database::GroupQueries::getGroupByName(groupName);

        if (!group) throw std::runtime_error("Group not found");

        const json data = {{"group", *group}};

        const json response = {
            {"command", "group.get.byName.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.get.byName.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}


void GroupHandler::handleUpdateGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can update groups");

        const auto payload = msg.at("payload");
        const unsigned int groupId = payload.at("id").get<unsigned int>();
        const std::string newName = payload.at("name").get<std::string>();

        if (!auth::AuthManager::isValidGroup(newName))
            throw std::runtime_error("Invalid group name: " + newName);

        const auto group = database::GroupQueries::getGroup(groupId);
        if (!group) throw std::runtime_error("Group not found");

        group->name = newName;

        if (payload.contains("linux_gid")) {
            const auto linux_gid = payload.at("linux_gid").get<unsigned int>();
            if (linux_gid <= 0)
                throw std::runtime_error("Invalid Linux GID: must be a positive integer");
            group->linux_gid = linux_gid;
        }

        database::GroupQueries::updateGroup(group);

        const json response = {
            {"command", "group.update.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", {{"id", groupId}, {"name", newName}}}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.update.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleListGroupsByUser(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can list groups by user");

        const auto userId = msg.at("payload").at("userId").get<unsigned int>();
        const auto groups = database::GroupQueries::listGroups(userId);

        const json data = {{"groups", groups}};

        const json response = {
            {"command", "groups.list.byUser.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "groups.list.byUser.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}
