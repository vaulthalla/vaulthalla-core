#pragma once

#include "storage/StorageEngine.hpp"

namespace vh::types {
struct File;
struct Vault;
struct Volume;
}

namespace vh::storage {

class CloudStorageEngine : public StorageEngine {
public:
    CloudStorageEngine() = default;

    ~CloudStorageEngine() override = default;

    CloudStorageEngine(const std::shared_ptr<types::Vault>& vault,
                       const std::vector<std::shared_ptr<types::Volume> >& volumes);

    void mountVolume(const std::shared_ptr<types::Volume>& volume) override;

    void unmountVolume(const std::shared_ptr<types::Volume>& volume) override;

    [[nodiscard]] StorageType type() const override { return StorageType::Cloud; }

    bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;

    [[nodiscard]] std::optional<std::vector<uint8_t> > readFile(const std::filesystem::path& rel_path) const override;

    bool deleteFile(const std::filesystem::path& rel_path) override;

    [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] std::vector<std::shared_ptr<types::File> > listFilesInDir(unsigned int volume_id,
                                                                            const std::filesystem::path& rel_path,
                                                                            bool recursive) const override;
};

} // namespace vh::storage