#include "protocols/ws/handler/share/Filesystem.hpp"

#include "db/encoding/timestamp.hpp"
#include "fs/model/Directory.hpp"
#include "fs/model/Entry.hpp"
#include "fs/model/File.hpp"
#include "protocols/ws/Session.hpp"
#include "share/Manager.hpp"
#include "share/Principal.hpp"
#include "share/TargetResolver.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace vh::protocols::ws::handler::share {
namespace share_fs_handler_detail {
[[nodiscard]] std::shared_ptr<vh::share::Manager> defaultManager() {
    return std::make_shared<vh::share::Manager>();
}

[[nodiscard]] std::shared_ptr<vh::share::TargetResolver> defaultResolver() {
    return std::make_shared<vh::share::TargetResolver>();
}

[[nodiscard]] Filesystem::ManagerFactory& managerFactory() {
    static Filesystem::ManagerFactory factory = defaultManager;
    return factory;
}

[[nodiscard]] Filesystem::ResolverFactory& resolverFactory() {
    static Filesystem::ResolverFactory factory = defaultResolver;
    return factory;
}

[[nodiscard]] std::shared_ptr<vh::share::Manager> manager() {
    auto instance = managerFactory()();
    if (!instance) throw std::runtime_error("Share manager is unavailable");
    return instance;
}

[[nodiscard]] std::shared_ptr<vh::share::TargetResolver> resolver() {
    auto instance = resolverFactory()();
    if (!instance) throw std::runtime_error("Share target resolver is unavailable");
    return instance;
}

[[nodiscard]] const json& objectPayload(const json& payload) {
    if (!payload.is_object()) throw std::invalid_argument("Share filesystem payload must be an object");
    return payload;
}

[[nodiscard]] std::string requestedPath(const json& payload, const vh::share::Principal& principal) {
    if (!payload.contains("path") || payload.at("path").is_null()) return principal.root_path;
    auto path = payload.at("path").get<std::string>();
    if (path == "/") return principal.root_path;
    return path;
}

void requireShareMode(const std::shared_ptr<Session>& session) {
    if (!session) throw std::runtime_error("Share filesystem commands require websocket session");
    if (session->user) throw std::runtime_error("Share filesystem commands require share mode");
    if (!session->isShareMode()) throw std::runtime_error("Share filesystem commands require verified share mode");
    if (session->shareSessionToken().empty()) throw std::runtime_error("Share session token is missing");
}

[[nodiscard]] std::shared_ptr<vh::share::Principal> refreshPrincipal(
    const std::shared_ptr<Session>& session,
    vh::share::Manager& manager
) {
    auto principal = manager.resolvePrincipal(
        session->shareSessionToken(),
        session->ipAddress.empty() ? std::nullopt : std::make_optional(session->ipAddress),
        session->userAgent.empty() ? std::nullopt : std::make_optional(session->userAgent)
    );
    session->setSharePrincipal(principal, session->shareSessionToken());
    return principal;
}

[[nodiscard]] json safeEntryJson(
    const vh::fs::model::Entry& entry,
    const vh::share::Principal& principal
) {
    json out = {
        {"id", entry.id},
        {"name", entry.name},
        {"path", vh::share::TargetResolver::shareRelativePath(principal, entry.path.string())},
        {"type", entry.isDirectory() ? "directory" : "file"},
        {"size_bytes", entry.size_bytes},
        {"created_at", db::encoding::timestampToString(entry.created_at)},
        {"updated_at", db::encoding::timestampToString(entry.updated_at)}
    };

    if (entry.isDirectory()) {
        const auto& dir = static_cast<const vh::fs::model::Directory&>(entry);
        out["file_count"] = dir.file_count;
        out["subdirectory_count"] = dir.subdirectory_count;
    } else {
        const auto& file = static_cast<const vh::fs::model::File&>(entry);
        out["mime_type"] = file.mime_type ? json(*file.mime_type) : json(nullptr);
    }
    return out;
}
}
namespace fs_detail = share_fs_handler_detail;

json Filesystem::metadata(const json& payload, const std::shared_ptr<Session>& session) {
    fs_detail::requireShareMode(session);
    const auto& body = fs_detail::objectPayload(payload);
    auto mgr = fs_detail::manager();
    auto principal = fs_detail::refreshPrincipal(session, *mgr);

    auto target = fs_detail::resolver()->resolve(*principal, {
        .path = fs_detail::requestedPath(body, *principal),
        .operation = vh::share::Operation::Metadata,
        .path_mode = vh::share::TargetPathMode::VaultRelative
    });

    return {{"entry", fs_detail::safeEntryJson(*target.entry, *principal)}};
}

json Filesystem::list(const json& payload, const std::shared_ptr<Session>& session) {
    fs_detail::requireShareMode(session);
    const auto& body = fs_detail::objectPayload(payload);
    auto mgr = fs_detail::manager();
    auto principal = fs_detail::refreshPrincipal(session, *mgr);
    auto resolver = fs_detail::resolver();

    auto target = resolver->resolve(*principal, {
        .path = fs_detail::requestedPath(body, *principal),
        .operation = vh::share::Operation::List,
        .path_mode = vh::share::TargetPathMode::VaultRelative,
        .expected_target_type = vh::share::TargetType::Directory
    });

    json entries = json::array();
    for (const auto& entry : resolver->listChildren(*principal, target))
        entries.push_back(fs_detail::safeEntryJson(*entry, *principal));

    return {
        {"path", target.share_path},
        {"entries", std::move(entries)}
    };
}

void Filesystem::setManagerFactoryForTesting(ManagerFactory factory) {
    if (!factory) throw std::invalid_argument("Share manager factory is required");
    fs_detail::managerFactory() = std::move(factory);
}

void Filesystem::resetManagerFactoryForTesting() {
    fs_detail::managerFactory() = fs_detail::defaultManager;
}

void Filesystem::setResolverFactoryForTesting(ResolverFactory factory) {
    if (!factory) throw std::invalid_argument("Share target resolver factory is required");
    fs_detail::resolverFactory() = std::move(factory);
}

void Filesystem::resetResolverFactoryForTesting() {
    fs_detail::resolverFactory() = fs_detail::defaultResolver;
}

}
