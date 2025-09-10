#pragma once

#include "CommandUsage.hpp"
#include "ArgsGenerator.hpp"

#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <sstream>
#include <optional>
#include <unordered_map>
#include <typeindex>
#include <variant>
#include <vector>
#include <algorithm>
#include <chrono>
#include <ranges>

namespace vh::test::cli {

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------
inline std::string dashify(std::string s) {
    std::replace(s.begin(), s.end(), '_', '-');
    return s;
}

inline uint64_t now_seed() {
    return static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

inline std::string usage_path_from(const std::vector<std::string>& path_aliases) {
    std::ostringstream oss; bool first = true;
    for (const auto& p : path_aliases) { if (!first) oss << '/'; first = false; oss << p; }
    return oss.str();
}

// Direct, low-friction value generation when nothing else provides a value
inline std::string generate_now(args::Gen g, std::string_view token, std::string_view usage_path) {
    args::Rng rng{now_seed()};
    args::GenContext ctx{std::string(token), std::string(usage_path)};
    return args::to_string_value(g->generate(rng, ctx));
}

// ------------------------------------------------------------
// CommandBuilder: usage-first command line synthesizer
// ------------------------------------------------------------
class CommandBuilder {
public:
    explicit CommandBuilder(std::shared_ptr<shell::CommandUsage> usage) : usage_(std::move(usage)) {}

    // Provide a global token → value override
    CommandBuilder& withOverride(std::string token, std::string value) {
        overrides_[std::move(token)] = std::move(value);
        return *this;
    }

    // Provide a value provider fallback (e.g., ArgsGenerator)
    CommandBuilder& withProvider(std::shared_ptr<args::ArgValueProvider> p) {
        provider_ = std::move(p);
        return *this;
    }

    // Attach an entity; we register a token-extractor binder for its type
    template <typename Entity>
    CommandBuilder& withEntity(const Entity& e) {
        const auto tid = std::type_index(typeid(Entity));
        binders_[tid] = [&e](const std::string& token) -> std::optional<std::string> {
            return extract(token, e);
        };
        return *this;
    }

    static std::vector<std::string> constructPath(const std::shared_ptr<shell::CommandUsage>& cmd) {
        std::vector<std::string> path_aliases;
        auto current = cmd;
        while (current) {
            // Prefer primary() if defined, otherwise first alias, otherwise the label
            std::string piece;
            if (!current->aliases.empty()) piece = current->aliases.front();
            else                           piece = current->primary(); // falls back to something sane in your impl
            path_aliases.push_back(std::move(piece));
            current = current->parent.lock();
        }
        std::ranges::reverse(path_aliases);
        return path_aliases;
    }

    // Build: "vh <path> [positionals] [flags] [options] [groups]"
    std::string build() const {
        std::ostringstream cmd;
        const auto path_aliases_ = constructPath(usage_);
        for (size_t i = 0; i < path_aliases_.size(); ++i) {
            if (i) cmd << ' ';
            cmd << path_aliases_[i];
        }
        const auto usage_path = usage_path_from(path_aliases_);

        // Positionals
        for (const auto& pos : usage_->positionals) {
            const auto val = resolve_any({pos.label}, pos.aliases, usage_path,
                                         /*positional_generator*/ pos.generator.get());
            if (val) cmd << ' ' << *val;
        }

        // Required flags (always emit best alias)
        for (const auto& f : usage_->required_flags) {
            if (auto sw = pick_flag_alias(f)) cmd << ' ' << *sw;
        }

        // Optional flags: emit if default_state is true or explicitly overridden
        for (const auto& f : usage_->optional_flags) {
            const bool emit = f.default_state || has_explicit_flag(f);
            if (!emit) continue;
            if (auto sw = pick_flag_alias(f)) cmd << ' ' << *sw;
        }

        // Required options
        for (const auto& o : usage_->required) {
            emit_option(cmd, o, usage_path);
        }

        // Optional options
        for (const auto& o : usage_->optional) {
            const bool has_default = o.default_value.has_value();
            const bool can_resolve = can_resolve_any(o.value_tokens, usage_path);
            if (has_default || can_resolve) emit_option(cmd, o, usage_path);
        }

        // Grouped items (Optional | Flag)
        for (const auto& g : usage_->groups) {
            for (const auto& item : g.items) {
                std::visit([&](auto&& it){ emit_group_item(cmd, it, usage_path); }, item);
            }
        }

        return cmd.str();
    }

private:
    std::shared_ptr<shell::CommandUsage> usage_;

    std::unordered_map<std::type_index, std::function<std::optional<std::string>(const std::string&)>> binders_;
    std::unordered_map<std::string, std::string> overrides_;
    std::shared_ptr<args::ArgValueProvider> provider_;

    // ---------- Resolution helpers ----------
    bool has_explicit_flag(const shell::Flag& f) const {
        // If any override matches the label or an alias treated as token name
        if (overrides_.count(f.label)) return true;
        for (const auto& a : f.aliases) if (overrides_.count(a)) return true;
        return false;
    }

    static std::optional<std::string> pick_flag_alias(const shell::Flag& f) {
        if (!f.aliases.empty()) {
            // Prefer long-form "--" alias, then the first
            auto it = std::find_if(f.aliases.begin(), f.aliases.end(), [](const std::string& s){ return s.rfind("--",0) == 0; });
            return (it != f.aliases.end()) ? std::optional<std::string>(*it) : std::optional<std::string>(f.aliases.front());
        }
        // Fall back to --<label>
        return std::string("--") + dashify(f.label);
    }

    bool can_resolve_any(const std::vector<std::string>& tokens, const std::string& usage_path) const {
        for (const auto& t : tokens) if (resolve_token(t, usage_path)) return true;
        return false;
    }

    std::optional<std::string> resolve_any(std::initializer_list<std::string> primary,
                                           const std::vector<std::string>& aliases,
                                           const std::string& usage_path,
                                           const args::IGenerator* positional_gen) const {
        // 1) overrides
        for (const auto& p : primary) if (auto it = overrides_.find(p); it != overrides_.end()) return it->second;
        for (const auto& a : aliases) if (auto it = overrides_.find(a); it != overrides_.end()) return it->second;

        // 2) entity binders
        for (const auto& p : primary) if (auto v = resolve_from_entities(p)) return v;
        for (const auto& a : aliases) if (auto v = resolve_from_entities(a)) return v;

        // 3) positional generator if provided
        if (positional_gen) {
            args::Rng rng{now_seed()};
            args::GenContext ctx{primary.begin()!=primary.end()?*primary.begin():std::string{"pos"}, usage_path};
            return args::to_string_value(positional_gen->generate(rng, ctx));
        }

        // 4) provider fallback
        if (provider_) {
            for (const auto& p : primary) if (auto v = provider_->valueFor(p, usage_path)) return v;
            for (const auto& a : aliases) if (auto v = provider_->valueFor(a, usage_path)) return v;
        }

        return std::nullopt;
    }

    std::optional<std::string> resolve_token(const std::string& token, const std::string& usage_path) const {
        if (auto it = overrides_.find(token); it != overrides_.end()) return it->second;
        if (auto v = resolve_from_entities(token)) return v;
        if (provider_) return provider_->valueFor(token, usage_path);
        return std::nullopt;
    }

    std::optional<std::string> resolve_from_entities(const std::string& token) const {
        for (const auto& [_, fn] : binders_) {
            if (auto v = fn(token)) return v;
        }
        return std::nullopt;
    }

    // ---------- Emitters ----------
    template <typename OptLike>
    void emit_option(std::ostringstream& cmd, const OptLike& o, const std::string& usage_path) const {
        const auto& opts = o.option_tokens;
        const auto& vals = o.value_tokens;

        if (opts.empty() || vals.empty()) return;

        // Case A: 1 option, many values → `--opt v1 v2 ...`
        if (opts.size() == 1 && vals.size() >= 1) {
            if (auto joined = resolve_values(vals, usage_path, o); joined) {
                cmd << ' ' << "--" << dashify(opts.front()) << ' ' << *joined;
            }
            return;
        }

        // Case B: many options, 1 value → `--a v` `--b v`
        if (vals.size() == 1 && opts.size() >= 1) {
            if (auto v = resolve_token(vals.front(), usage_path)) {
                for (const auto& opt : opts) cmd << ' ' << "--" << dashify(opt) << ' ' << *v;
            } else if constexpr (std::is_same_v<OptLike, shell::Optional>) {
                if (o.default_value) {
                    for (const auto& opt : opts) cmd << ' ' << "--" << dashify(opt) << ' ' << *o.default_value;
                }
            }
            return;
        }

        // Case C: parallel vectors, pairwise
        const size_t n = std::min(opts.size(), vals.size());
        for (size_t i = 0; i < n; ++i) {
            auto v = resolve_token(vals[i], usage_path);
            if constexpr (std::is_same_v<OptLike, shell::Optional>) {
                if (!v && o.default_value) v = o.default_value;
            }
            if (v) cmd << ' ' << "--" << dashify(opts[i]) << ' ' << *v;
        }
    }

    static void emit_group_item(std::ostringstream& cmd, const shell::Flag& f, const std::string&) {
        if (f.default_state) {
            if (const auto sw = pick_flag_alias(f)) cmd << ' ' << *sw;
        }
    }

    void emit_group_item(std::ostringstream& cmd, const shell::Optional& o, const std::string& usage_path) const {
        emit_option(cmd, o, usage_path);
    }

    template <typename T>
    static constexpr bool constexpr_has_default() { return std::is_same_v<T, shell::Optional>; }

    std::optional<std::string> resolve_values(const std::vector<std::string>& tokens,
                                          const std::string& usage_path,
                                          const shell::Optional& o) const {
        std::ostringstream oss; bool first = true; bool any = false;
        for (const auto& t : tokens) {
            if (auto v = resolve_token(t, usage_path)) {
                if (!first) oss << ' ';
                first = false;
                oss << *v;
                any = true;
            } else if (o.default_value) {
                if (!first) oss << ' ';
                first = false;
                oss << *o.default_value;
                any = true;
            }
        }
        if (!any) return std::nullopt;
        return oss.str();
    }

    std::optional<std::string> resolve_values(const std::vector<std::string>& tokens,
                                              const std::string& usage_path,
                                              const shell::Option&) const {
        std::ostringstream oss; bool first = true; bool any = false;
        for (const auto& t : tokens) {
            if (auto v = resolve_token(t, usage_path)) {
                if (!first) oss << ' ';
                first = false;
                oss << *v;
                any = true;
            }
        }
        if (!any) return std::nullopt;
        return oss.str();
    }

    // ---------- Entity token extractors ----------
    static std::optional<std::string> extract(const std::string& token, const types::User& u) {
        if (token == "name" || token == "username") return u.name;
        if (token == "email") return u.email.value_or("");
        if (token == "uid" || token == "id" || token == "user_id") return std::to_string(u.id);
        if (token == "role" || token == "role_id") return u.role->name;
        return std::nullopt;
    }

    static std::optional<std::string> extract(const std::string& token, const types::Vault& v) {
        if (token == "name" || token == "vault_name") return v.name;
        if (token == "id" || token == "vault_id") return std::to_string(v.id);
        if (token == "owner" || token == "owner_id") return std::to_string(v.owner_id);
        if (token == "quota") return std::to_string(v.quota);
        return std::nullopt;
    }

    static std::optional<std::string> extract(const std::string& token, const types::Group& g) {
        if (token == "name" || token == "group_name") return g.name;
        if (token == "id" || token == "group_id") return std::to_string(g.id);
        return std::nullopt;
    }

    static std::optional<std::string> extract(const std::string& token, const types::Role& r) {
        if (token == "name" || token == "role_name") return r.name;
        if (token == "id" || token == "role_id") return std::to_string(r.id);
        return std::nullopt;
    }

    static std::optional<std::string> extract(const std::string& token, const types::UserRole& ur) {
        if (token == "user_id") return std::to_string(ur.user_id);
        if (token == "role_id") return std::to_string(ur.id);
        return std::nullopt;
    }

    static std::optional<std::string> extract(const std::string& token, const types::VaultRole& vr) {
        if (token == "vault_id") return std::to_string(vr.vault_id);
        if (token == "role_id") return std::to_string(vr.role_id);
        return std::nullopt;
    }
};

}
