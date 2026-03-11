#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Module.hpp"
#include "rbac/permission/Override.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace vh::rbac::permission::vault {

enum class SyncConfigPermissions : uint8_t {
    None = 0,
    View = 1 << 0,
    Edit = 1 << 1,
    All = View | Edit,
};

enum class SyncActionPermissions : uint8_t {
    None = 0,
    Trigger = 1 << 0,
};

struct SyncConfig : Set<SyncConfigPermissions, uint8_t> {
    [[nodiscard]] bool canView() const noexcept { return has(SyncConfigPermissions::View); }
    [[nodiscard]] bool canEdit() const noexcept { return has(SyncConfigPermissions::Edit); }
    [[nodiscard]] bool all() const noexcept { return has(SyncConfigPermissions::All); }
    [[nodiscard]] bool none() const noexcept { return has(SyncConfigPermissions::None); }
};

struct SyncAction : Set<SyncActionPermissions, uint8_t> {
    [[nodiscard]] bool canTrigger() const noexcept { return has(SyncActionPermissions::Trigger); }
    [[nodiscard]] bool none() const noexcept { return has(SyncActionPermissions::None); }
};

struct Sync final : Module<uint32_t> {
    static constexpr const auto* ModuleName = "Sync";

    SyncConfig config;
    SyncAction action;
    std::vector<std::shared_ptr<Override>> overrides;

    Sync() = default;
    explicit Sync(const Mask& mask) { fromMask(mask); }

    const char* name() const override { return ModuleName; }
    [[nodiscard]] uint32_t toMask() const override { return pack(config, action); }
    void fromMask(const Mask mask) override { unpack(mask, config, action); }

    [[nodiscard]] bool canViewConfig() const noexcept { return config.canView(); }
    [[nodiscard]] bool canEditConfig() const noexcept { return config.canEdit(); }
    [[nodiscard]] bool canTriggerSync() const noexcept { return action.canTrigger(); }
};

}