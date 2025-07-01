#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
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
    StorageEngine(const std::shared_ptr<types::Vault>& vault,
                  const std::vector<std::shared_ptr<types::Volume> >& volumes,
                  fs::path root_mount_path = fs::path("/"))
        : vault_(vault), volumes_(volumes), root_(std::move(root_mount_path)) {
    }

    virtual ~StorageEngine() = default;

    [[nodiscard]] std::shared_ptr<types::Vault> getVault() const { return vault_; }
    [[nodiscard]] std::vector<std::shared_ptr<types::Volume> > getVolumes() const { return volumes_; }

    virtual void mountVolume(const std::shared_ptr<types::Volume>& volume) = 0;

    virtual void unmountVolume(const std::shared_ptr<types::Volume>& volume) = 0;

    virtual void mkdir(unsigned int volumeId, const fs::path& relative_path) = 0;

    [[nodiscard]] virtual bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data,
                                         bool overwrite) = 0;

    [[nodiscard]] virtual std::optional<std::vector<uint8_t> > readFile(const fs::path& relative_path) const = 0;

    [[nodiscard]] virtual bool deleteFile(const fs::path& relative_path) = 0;

    [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const = 0;

    [[nodiscard]] virtual std::vector<std::shared_ptr<types::File> > listFilesInDir(unsigned int volume_id,
        const std::filesystem::path& rel_path,
        bool recursive) const = 0;

    [[nodiscard]] fs::path root_directory() const;

    [[nodiscard]] virtual StorageType type() const = 0;

protected:
    std::shared_ptr<types::Vault> vault_;
    std::vector<std::shared_ptr<types::Volume> > volumes_;
    fs::path root_;
};

} // namespace vh::storage