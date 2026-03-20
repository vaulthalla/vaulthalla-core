#include "fuse/Resolver.hpp"
#include "db/query/identities/User.hpp"
#include "db/query/identities/Group.hpp"
#include "runtime/Deps.hpp"
#include "log/Registry.hpp"
#include "fs/cache/Registry.hpp"
#include "fs/model/Entry.hpp"
#include "storage/Manager.hpp"
#include "rbac/resolver/vault/all.hpp"
#include "fs/model/Path.hpp"

namespace vh::fuse {
    using resolver::Target;
    using resolver::Request;
    using resolver::Resolved;
    using resolver::Status;

    Resolved Resolver::resolve(const Request &req) {
        Resolved res;

        if (!req.fuseReq) {
            res.setStatus(Status::MissingFuseContext, EINVAL);
            return res;
        }

        constexpr std::array functions{
            resolveIdentity,
            resolveEntry,
            resolveEngine,
            resolvePath,
            enforcePermissions
        };

        for (const auto &func: functions)
            if (!func(req, res) || !res.ok())
                break;

        return res;
    }

    bool Resolver::resolveIdentity(const resolver::Request &req, resolver::Resolved &out) {
        const fuse_ctx *fctx = fuse_req_ctx(req.fuseReq);
        if (!fctx) {
            out.setStatus(Status::MissingFuseContext, EINVAL);
            return false;
        }

        const uid_t uid = fctx->uid;
        const gid_t gid = fctx->gid;

        out.user = db::query::identities::User::getUserByLinuxUID(uid);
        if (!out.user) {
            log::Registry::fuse()->error("[{}] No user found for UID {}", req.caller, uid);
            out.status = Status::MissingUser;
            return false;
        }

        out.group = db::query::identities::Group::getGroupByLinuxGID(gid);

        return true;
    }

    bool Resolver::resolveParentEntry(const resolver::Request &req, resolver::Resolved &out) {
        if (resolver::hasFlag(req.target, Target::Path) || resolver::hasFlag(req.target, Target::EntryForPath)) {
            if (!req.parentIno) {
                log::Registry::fuse()->debug("[{}] Request target includes ParentEntry but no parent inode provided",
                                             req.caller);
                out.status = Status::MissingParentIno;
                return false;
            }

            out.parentEntry = runtime::Deps::get().fsCache->getEntry(*req.parentIno);
            if (!out.parentEntry) {
                log::Registry::fuse()->error("[{}] Failed to resolve parent entry", req.caller);
                out.setStatus(Status::MissingParentEntry, ENOENT);
                return false;
            }
        }

        return true;
    }

    bool Resolver::resolveEntry(const resolver::Request &req, resolver::Resolved &out) {
        if (resolver::hasFlag(req.target, Target::Entry)) {
            if (!req.ino) {
                log::Registry::fuse()->debug("[{}] Request target includes Ino but no inode provided", req.caller);
                out.status = Status::MissingIno;
                return false;
            }

            out.entry = runtime::Deps::get().fsCache->getEntry(*req.ino);
            if (!out.entry) {
                log::Registry::fuse()->error("[{}] Failed to resolve entry", req.caller);
                out.setStatus(Status::MissingEntry, ENOENT);
                return false;
            }
        } else if (resolver::hasFlag(req.target, Target::Path) || resolver::hasFlag(req.target, Target::EntryForPath)) {
            if (!out.parentEntry) {
                log::Registry::fuse()->error("[{}] Cannot resolve path without parent entry", req.caller);
                out.setStatus(Status::MissingParentEntry, ENOENT);
                return false;
            }

            if (!req.childName) {
                log::Registry::fuse()->debug("[{}] Request target includes Child but no child name provided", req.caller);
                out.setStatus(Status::MissingChildName, ENOENT);
                return false;
            }

            out.path = out.parentEntry->path / *req.childName;
            out.fusePath = out.engine->paths->absRelToAbsRel(*out.path, fs::model::PathType::VAULT_ROOT,
                                                                 fs::model::PathType::FUSE_ROOT);

            if (resolver::hasFlag(req.target, Target::EntryForPath)) {
                out.entry = runtime::Deps::get().fsCache->getEntry(*out.fusePath);
                if (!out.entry) {
                    log::Registry::fuse()->error("[{}] Failed to resolve entry for path {}", req.caller, out.fusePath->string());
                    out.setStatus(Status::MissingEntry, ENOENT);
                    return false;
                }
            } else out.ino = runtime::Deps::get().fsCache->getOrAssignInode(*out.fusePath);
        }


        return true;
    }

    bool Resolver::resolvePath(const resolver::Request &req, resolver::Resolved &out) {
        if (resolver::hasFlag(req.target, Target::Path)) {
            if (!req.childName) {
                log::Registry::fuse()->debug("[{}] Request target includes Child but no child name provided",
                                             req.caller);
                out.setStatus(Status::MissingChildName, EINVAL);
                return false;
            }

            if (out.entry->path.empty()) {
                log::Registry::fuse()->error("[{}] Failed to resolve parent path for inode {}", req.caller, *req.ino);
                out.setStatus(Status::MissingParentPath, ENOENT);
                return false;
            }

            out.path = out.entry->path / *req.childName;
            if (!out.path || out.path->empty()) {
                log::Registry::fuse()->error("[{}] Failed to resolve child path for parent inode {} and child name {}",
                                             req.caller, *req.ino, *req.childName);
                out.setStatus(Status::InvalidChildPath, ENOENT);
                return false;
            }

            out.fusePath = out.engine->paths->absRelToAbsRel(*out.path, fs::model::PathType::VAULT_ROOT,
                                                             fs::model::PathType::FUSE_ROOT);
            out.ino = runtime::Deps::get().fsCache->getOrAssignInode(*out.path);
        };

        return true;
    }

    static bool enforcePermission(const std::shared_ptr<vh::identities::User> &user,
                                  const rbac::permission::vault::FilesystemAction &action,
                                  const std::shared_ptr<fs::model::Entry> &entry,
                                  const std::optional<std::filesystem::path> &path = std::nullopt) {
        if (!rbac::resolver::Vault::has<rbac::permission::vault::FilesystemAction>({
            .user = user,
            .permission = action,
            .path = path,
            .entry = entry
        })) {
            if (entry) log::Registry::fuse()->error("[create] Access denied for user {} on path {}", user->name,
                                                    entry->path.string());
            else if (path) log::Registry::fuse()->error("[create] Access denied for user {} on path {}", user->name,
                                                        path->string());
            return false;
        }

        return true;
    }

    bool Resolver::enforcePermissions(const resolver::Request &req, resolver::Resolved &out) {
        if (req.action) {
            if (resolver::hasFlag(req.target, Target::Entry) && !enforcePermission(out.user, *req.action, out.entry)) {
                out.setStatus(Status::AccessDenied, EPERM);
                return false;
            }

            if (resolver::hasFlag(req.target, Target::Path) && !enforcePermission(
                    out.user, *req.action, nullptr, out.path)) {
                out.setStatus(Status::AccessDenied, EPERM);
                return false;
            }
        }

        if (!req.actions.empty()) {
            for (const auto &action: req.actions) {
                if (resolver::hasFlag(req.target, Target::Entry) && !enforcePermission(out.user, action, out.entry)) {
                    out.setStatus(Status::AccessDenied, EPERM);
                    return false;
                }

                if (resolver::hasFlag(req.target, Target::Path) && !enforcePermission(
                        out.user, action, nullptr, out.path)) {
                    out.setStatus(Status::AccessDenied, EPERM);
                    return false;
                }
            }
        }

        return true;
    }

    bool Resolver::resolveEngine(const resolver::Request &req, resolver::Resolved &out) {
        (void) req;
        if (out.entry && out.entry->vault_id)
            out.engine = runtime::Deps::get().storageManager->getEngine(*out.entry->vault_id);

        return !!out.engine;
    }
}
