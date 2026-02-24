#pragma once

#include <memory>

namespace vh::crypto::model { struct InternalSecret; }

namespace vh::database {

struct InternalSecretQueries {
    static void upsertSecret(const std::shared_ptr<crypto::model::InternalSecret>& secret);
    static std::shared_ptr<crypto::model::InternalSecret> getSecret(const std::string& key);
    [[nodiscard]] static bool secretExists(const std::string& key);
};

}
