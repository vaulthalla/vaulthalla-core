#pragma once

#include "storage/StorageEngine.hpp"

namespace vh::types {
struct File;
struct Vault;
}

namespace vh::storage {

class CloudStorageEngine : public StorageEngine {
public:
    CloudStorageEngine() = default;

    ~CloudStorageEngine() override = default;

    explicit CloudStorageEngine(const std::shared_ptr<types::Vault>& vault);

    void mkdir(const fs::path& relative_path) override;

    [[nodiscard]] StorageType type() const override { return StorageType::Cloud; }

    bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;

    [[nodiscard]] std::optional<std::vector<uint8_t> > readFile(const std::filesystem::path& rel_path) const override;

    bool deleteFile(const std::filesystem::path& rel_path) override;

    [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;
};

} // namespace vh::storage