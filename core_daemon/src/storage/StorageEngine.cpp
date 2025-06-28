#include "storage/StorageEngine.hpp"
#include "types/db/Vault.hpp"
#include "types/db/Volume.hpp"

namespace vh::storage {

fs::path StorageEngine::root_directory() const { return root_; }

} // namespace vh::storage
