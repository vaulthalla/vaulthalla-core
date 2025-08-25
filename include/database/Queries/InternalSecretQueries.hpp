#pragma once

#include <memory>

namespace vh::types {
    struct InternalSecret;
}

namespace vh::database {

struct InternalSecretQueries {
    static void upsertSecret(const std::shared_ptr<types::InternalSecret>& secret);
    static std::shared_ptr<types::InternalSecret> getSecret(const std::string& key);
    [[nodiscard]] static bool secretExists(const std::string& key);
};

}
