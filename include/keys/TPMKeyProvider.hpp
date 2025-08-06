#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>

namespace vh::crypto {

class TPMKeyProvider {
public:
    explicit TPMKeyProvider(std::string sealedBlobPath = "/var/lib/vaulthalla/sealed_master.blob");

    void init();  // check or generate
    [[nodiscard]] const std::vector<uint8_t>& getMasterKey() const;

private:
    std::vector<uint8_t> masterKey_;
    std::string sealedBlobPath_;
    std::filesystem::path sealedDir_;

    void generate_and_seal();
    void unseal();
};

} // namespace vh::crypto
