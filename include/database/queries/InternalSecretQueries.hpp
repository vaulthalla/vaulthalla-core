#pragma once

#include <memory>

namespace vh::crypto::model { struct Secret; }

namespace vh::database {

struct SecretQueries {
    static void upsertSecret(const std::shared_ptr<crypto::model::Secret>& secret);
    static std::shared_ptr<crypto::model::Secret> getSecret(const std::string& key);
    [[nodiscard]] static bool secretExists(const std::string& key);
};

}
