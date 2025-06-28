#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

namespace vh::types {
struct Vault;
struct Volume;
struct File;
}

namespace vh::storage {

enum class StorageType { Local, Cloud };

class StorageEngine {
  public:
    StorageEngine() = default;
    virtual ~StorageEngine() = default;

    [[nodiscard]] std::shared_ptr<types::Vault> getVault() const { return vault_; }
    [[nodiscard]] std::vector<std::shared_ptr<types::Volume>> getVolumes() const { return volumes_; }

    virtual void mountVolume(const std::shared_ptr<types::Volume>& volume) = 0;
    virtual void unmountVolume(const std::shared_ptr<types::Volume>& volume) = 0;

    [[nodiscard]] virtual bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data, bool overwrite) = 0;
    [[nodiscard]] virtual std::optional<std::vector<uint8_t>> readFile(const fs::path& relative_path) const = 0;
    [[nodiscard]] virtual bool deleteFile(const fs::path& relative_path) = 0;
    [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const = 0;
    virtual std::vector<std::shared_ptr<types::File>> listFilesInDir(const std::filesystem::path& rel_path,
                                                                    bool recursive) const = 0;

    fs::path root_directory() const;
    virtual StorageType type() const = 0;

private:
    std::shared_ptr<types::Vault> vault_;
    std::vector<std::shared_ptr<types::Volume>> volumes_;
    fs::path root_;
};

} // namespace vh::storage
