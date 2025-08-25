#pragma once

#include "TPMKeyProvider.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace vh::crypto {

class InternalSecretManager {
public:
    InternalSecretManager();

    std::string jwtSecret() const;
    void setJWTSecret(const std::string& secret) const;

private:
    mutable std::mutex mutex_;
    std::unique_ptr<TPMKeyProvider> tpmKeyProvider_;

    std::string getOrInitSecret(const std::string& key) const;
    void setEncryptedValue(const std::string& key, const std::string& value) const;
};

}
