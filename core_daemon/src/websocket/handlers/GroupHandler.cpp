#include "websocket/handlers/GroupHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "types/db/User.hpp"
#include "types/db/Group.hpp"

#include <nlohmann/json.hpp>

using namespace vh::websocket;

void GroupHandler::handleCreateGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can create groups");

        const auto payload = msg.at("payload");
        const std::string groupName = payload.at("name").get<std::string>();
        const std::string groupDescription = payload.value("description", "");

        database::GroupQueries::createGroup(groupName, groupDescription);

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
        if (!user || !user->canManageUsers())
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
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can add members to groups");

        const auto payload = msg.at("payload");
        const unsigned int groupId = payload.at("groupId").get<unsigned int>();
        const std::string memberName = payload.at("memberName").get<std::string>();

        database::GroupQueries::addMemberToGroup(groupId, memberName);

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
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can remove members from groups");

        const auto payload = msg.at("payload");
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
        if (!user || !user->canManageUsers())
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
        if (!user || !user->canManageUsers())
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
        if (!user || !user->canManageUsers())
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
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can update groups");

        const auto payload = msg.at("payload");
        const unsigned int groupId = payload.at("id").get<unsigned int>();
        const std::string newName = payload.at("name").get<std::string>();

        database::GroupQueries::updateGroup(groupId, newName);

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

void GroupHandler::handleAddStorageVolumeToGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can add storage volumes to groups");

        const auto payload = msg.at("payload");
        const unsigned int groupId = payload.at("groupId").get<unsigned int>();
        const unsigned int volumeId = payload.at("volumeId").get<unsigned int>();

        database::GroupQueries::addStorageVolumeToGroup(groupId, volumeId);

        const json response = {
            {"command", "group.volume.add.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", {{"groupId", groupId}, {"volumeId", volumeId}}}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.volume.add.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleRemoveStorageVolumeFromGroup(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can remove storage volumes from groups");

        const auto payload = msg.at("payload");
        const unsigned int groupId = payload.at("groupId").get<unsigned int>();
        const unsigned int volumeId = payload.at("volumeId").get<unsigned int>();

        database::GroupQueries::removeStorageVolumeFromGroup(groupId, volumeId);

        const json response = {
            {"command", "group.volume.remove.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", {{"groupId", groupId}, {"volumeId", volumeId}}}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.volume.remove.response"},
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
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can list groups by user");

        const auto userId = msg.at("payload").at("userId").get<unsigned int>();
        const auto groups = database::GroupQueries::listGroupsByUser(userId);

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

void GroupHandler::handleListGroupsByStorageVolume(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can list groups by storage volume");

        const auto volumeId = msg.at("payload").at("volumeId").get<unsigned int>();
        const auto groups = database::GroupQueries::listGroupsByStorageVolume(volumeId);

        const json data = {{"groups", groups}};

        const json response = {
            {"command", "groups.list.byVolume.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "groups.list.byVolume.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void GroupHandler::handleGetGroupByStorageVolume(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageUsers())
            throw std::runtime_error("Permission denied: Only admins can get group by storage volume");

        const auto volumeId = msg.at("payload").at("volumeId").get<unsigned int>();
        auto group = database::GroupQueries::getGroupByStorageVolume(volumeId);

        if (!group) throw std::runtime_error("Group not found for the specified storage volume");

        const json data = {{"group", *group}};

        const json response = {
            {"command", "group.get.byVolume.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "group.get.byVolume.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}
