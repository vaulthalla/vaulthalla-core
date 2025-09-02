#pragma once

#include "CommandUsage.hpp"
#include "CommandBook.hpp"

namespace vh::shell {

namespace aku { [[nodiscard]] std::string usage_cloud_provider(); }

namespace permissions {
    [[nodiscard]] std::string usage_vault_permissions();
    [[nodiscard]] std::string usage_user_permissions();
}

namespace vault { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace user { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace group { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace secrets { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace aku { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace role { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace permissions { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace help { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace version { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }

}
