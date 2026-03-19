#pragma once

#include "rbac/fs/policy/Evaluator.hpp"
#include "rbac/resolver/vault/Context.hpp"
#include "rbac/resolver/vault/ResolvedContext.hpp"
#include "rbac/resolver/vault/policy/Base.hpp"
#include "log/Registry.hpp"

namespace vh::rbac::resolver::vault {
    template<>
    struct ContextPolicy<permission::vault::FilesystemAction> {
        static bool validate(
            const std::shared_ptr<identities::User> &actor,
            const ResolvedContext &resolved,
            const permission::vault::FilesystemAction action,
            const Context<permission::vault::FilesystemAction> &ctx
        ) {
            if (!actor) {
                log::Registry::auth()->warn("Access denied for unauthenticated user");
                return false;
            }

            if (!resolved.isValid()) return false;

            const fs::policy::Request req{
                .user = actor,
                .action = action,
                .vaultId = ctx.vault_id,
                .path = ctx.path,
                .entry = ctx.entry ? *ctx.entry : nullptr
            };

            const auto decision = fs::policy::Evaluator::evaluate(req);

            if (!decision.allowed) {
                if (ctx.path) {
                    log::Registry::auth()->warn(
                        "Access denied for user {} on path {} with decision context:\n{}",
                        actor->name,
                        ctx.path->string(),
                        decision.toString()
                    );
                } else {
                    log::Registry::auth()->warn(
                        "Access denied for user {} with decision context:\n{}",
                        actor->name,
                        decision.toString()
                    );
                }
            }

            return decision.allowed;
        }
    };
}
