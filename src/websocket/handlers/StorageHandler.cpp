#include "websocket/handlers/StorageHandler.hpp"
#include "storage/StorageManager.hpp"
#include "websocket/WebSocketSession.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "types/Vault.hpp"
#include "keys/APIKeyManager.hpp"
#include "types/User.hpp"
#include "types/APIKey.hpp"

#include <boost/algorithm/string.hpp>

#include <iostream>

namespace vh::websocket {

    StorageHandler::StorageHandler(const std::shared_ptr<vh::storage::StorageManager> &storageManager)
            : storageManager_(storageManager), apiKeyManager_(std::make_shared<keys::APIKeyManager>()) {}

    void StorageHandler::handleAddAPIKey(const json& msg, WebSocketSession& session) {
        try {
            json payload = msg.at("payload");
            unsigned short userID = payload.at("user_id").get<unsigned short>();
            std::string name = payload.at("name").get<std::string>();
            std::string type = payload.at("type").get<std::string>();
            std::string typeLower = boost::algorithm::to_lower_copy(type);

            std::shared_ptr<vh::types::api::APIKey> key;

            if (typeLower == "s3") {
                std::string provider = payload.at("provider").get<std::string>();
                std::string accessKey = payload.at("access_key").get<std::string>();
                std::string secretKey = payload.at("secret_access_key").get<std::string>();
                std::string region = payload.at("region").get<std::string>();
                std::string endpoint = payload.at("endpoint").get<std::string>();

                key = std::make_shared<vh::types::api::S3APIKey>(name, userID, provider, accessKey, secretKey, region, endpoint);
            } else throw std::runtime_error("Unsupported API key type: " + type);

            apiKeyManager_->addAPIKey(key);

            json response = {
                    {"command", "storage.apiKey.add.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Added API key: " << name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleAddAPIKey error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.apiKey.add.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleRemoveAPIKey(const json& msg, WebSocketSession& session) {
        try {
            unsigned int keyId = msg.at("payload").at("keyId").get<unsigned int>();
            const auto user = session.getAuthenticatedUser();
            apiKeyManager_->removeAPIKey(keyId, user->id);

            json response = {
                    {"command", "storage.apiKey.remove.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Removed API key with ID: " << keyId << "\n";

        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleRemoveAPIKey error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.apiKey.remove.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleListAPIKeys(const json& msg, WebSocketSession& session) {
        try {
            auto keys = apiKeyManager_->listAPIKeys();

            json data = {
                    {"keys", vh::types::api::to_json(keys).dump(4)}
            };

            json response = {
                    {"command", "storage.apiKey.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed API keys for all users." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListAPIKeys error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.apiKey.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void
    StorageHandler::handleListUserAPIKeys(const vh::websocket::json &msg, vh::websocket::WebSocketSession &session) {
        try {
            const auto user = session.getAuthenticatedUser();
            if (!user) throw std::runtime_error("User not authenticated");
            auto keys = apiKeyManager_->listUserAPIKeys(user->id);

            json data {
                    {"keys", vh::types::api::to_json(keys).dump(4)}
            };

            json response = {
                    {"command", "storage.apiKey.list.user.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed API keys for user ID: " << user->id << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListUserAPIKeys error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.apiKey.list.user.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleGetAPIKey(const json& msg, WebSocketSession& session) {
        try {
            unsigned int keyId = msg.at("payload").at("keyId").get<unsigned int>();
            const auto user = session.getAuthenticatedUser();
            auto key = apiKeyManager_->getAPIKey(keyId, user->id);

            json data = {
                    {"key", vh::types::api::to_json(key)}
            };

            json response = {
                    {"command", "storage.apiKey.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Fetched API key with ID: " << keyId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleGetAPIKey error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.apiKey.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleAddVault(const json& msg, WebSocketSession& session) {
        try {
            if (vh::database::VaultQueries::localDiskVaultExists())
                throw std::runtime_error("Local disk vault already exists. Only one local disk vault is allowed.");

            const json& payload = msg.at("payload");
            const std::string name = payload.at("name").get<std::string>();
            const std::string type = payload.at("type").get<std::string>();
            const std::string typeLower = boost::algorithm::to_lower_copy(type);

            std::unique_ptr<vh::types::Vault> vault;

            if (typeLower == "local") {
                const std::string mountPoint = payload.at("mountPoint").get<std::string>();
                vault = std::make_unique<vh::types::LocalDiskVault>(name, mountPoint);
            } else if (typeLower == "s3") {
                unsigned short apiKeyID = payload.at("apiKeyID").get<unsigned short>();
                const std::string bucket = payload.at("bucket").get<std::string>();
                vault = std::make_unique<vh::types::S3Vault>(name, apiKeyID, bucket);
            } else throw std::runtime_error("Unsupported vault type: " + typeLower);

            storageManager_->addVault(std::move(vault));

            json data = {
                    {"id", vault->id},
                    {"name", vault->name},
                    {"type", to_string(vault->type)},
                    {"isActive", vault->is_active},
                    {"createdAt", vault->created_at}
            };

            json response = {
                    {"command", "storage.vault.add.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Mounted vault: " << name << " -> " << type << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleInitLocalDisk error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.vault.add.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleRemoveVault(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("id").get<unsigned int>();
            storageManager_->removeVault(vaultId);

            json response = {
                    {"command", "storage.vault.remove.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Removed local disk vault with ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleRemoveLocalDiskVault error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.vault.remove.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleGetVault(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("id").get<unsigned int>();
            auto vault = storageManager_->getVault(vaultId);

            json data = {
                    {"vault", json(*vault)}
            };

            json response = {
                    {"command", "storage.vault.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Fetched local disk vault with ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleGetLocalDiskVault error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.vault.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleListVaults(const json& msg, WebSocketSession& session) {
        try {
            const auto vaults = storageManager_->listVaults();

            json data = {
                    {"vaults", vh::types::to_json(vaults).dump(4)}
            };

            json response = {
                    {"command", "storage.vault.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed S3 vaults.\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListS3Vaults error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.vault.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleAddVolume(const json& msg, WebSocketSession& session) {
        try {
            json payload = msg.at("payload");
            unsigned int userID = payload.at("user_id").get<unsigned int>();
            unsigned int vaultID = payload.at("vault_id").get<unsigned int>();
            std::string name = payload.at("name").get<std::string>();
            std::string pathPrefix = payload.contains("path_prefix") ? payload.at("path_prefix").get<std::string>() : "/";
            unsigned long long quotaBytes = payload.contains("quota_bytes") ? payload.at("quota_bytes").get<unsigned long long>() : 0;

            auto storageVolume = std::make_shared<vh::types::StorageVolume>(vaultID, name, pathPrefix, quotaBytes);
            storageManager_->addVolume(storageVolume, userID);

            json response = {
                    {"command", "storage.volume.add.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Added volume: " << name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleAddVolume error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.volume.add.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleRemoveVolume(const json& msg, WebSocketSession& session) {
        try {
            unsigned int volumeId = msg.at("payload").at("volumeId").get<unsigned int>();
            const auto user = session.getAuthenticatedUser();

            storageManager_->removeVolume(volumeId, user->id);

            json response = {
                    {"command", "storage.volume.remove.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Removed volume with ID: " << volumeId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleRemoveVolume error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.volume.remove.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleListUserVolumes(const json& msg, WebSocketSession& session) {
        try {
            unsigned int userId = msg.at("payload").at("userId").get<unsigned int>();
            auto volumes = database::VaultQueries::listUserVolumes(userId);

            json data = {
                    {"volumes", vh::types::to_json(volumes).dump(4)}
            };

            json response = {
                    {"command", "storage.volume.list.user.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed volumes for user ID: " << userId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListVolumes error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.volume.list.user.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleListVaultVolumes(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("vaultId").get<unsigned int>();
            auto volumes = database::VaultQueries::listVaultVolumes(vaultId);

            json data = {
                    {"volumes", vh::types::to_json(volumes).dump(4)}
            };

            json response = {
                    {"command", "storage.volume.list.vault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed volumes for vault ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListVolumes error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.volume.list.vault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleListVolumes(const vh::websocket::json &msg, vh::websocket::WebSocketSession &session) {
        try {
            auto volumes = database::VaultQueries::listVolumes();

            json data = {
                    {"volumes", vh::types::to_json(volumes).dump(4)}
            };

            json response = {
                    {"command", "storage.volume.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed all storage volumes.\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListVolumes error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.volume.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleGetVolume(const json& msg, WebSocketSession& session) {
        try {
            unsigned int volumeId = msg.at("payload").at("volumeId").get<unsigned int>();
            const auto user = session.getAuthenticatedUser();
            auto volume = storageManager_->getVolume(volumeId, user->id);

            json response = {
                    {"command", "storage.volume.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", *volume}
            };

            session.send(response);

            std::cout << "[StorageHandler] Fetched volume with ID: " << volumeId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleGetVolume error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.volume.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

} // namespace vh::websocket
