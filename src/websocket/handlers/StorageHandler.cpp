#include "websocket/handlers/StorageHandler.hpp"
#include "storage/StorageManager.hpp"
#include "websocket/WebSocketSession.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "types/Vault.hpp"

#include <iostream>

namespace vh::websocket {

    StorageHandler::StorageHandler(const std::shared_ptr<vh::storage::StorageManager> &storageManager)
            : storageManager_(storageManager) {}

    void StorageHandler::handleAddS3APIKey(const json& msg, WebSocketSession& session) {
        try {
            json payload = msg.at("payload");
            unsigned short userID = payload.at("userID").get<unsigned short>();
            std::string name = payload.at("name").get<std::string>();
            std::string accessKey = payload.at("accessKey").get<std::string>();
            std::string secretKey = payload.at("secretKey").get<std::string>();
            std::string region = payload.at("region").get<std::string>();
            std::string endpoint = payload.value("endpoint", "");

            auto apiKey = vh::types::S3APIKey(name, userID, accessKey, secretKey, region, endpoint);
            vh::database::APIKeyQueries::addS3APIKey(apiKey);

            json response = {
                    {"command", "storage.s3.addApiKey.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Added S3 API key: " << name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleAddS3APIKey error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.addApiKey.response"},
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

            vh::database::APIKeyQueries::removeAPIKey(keyId);

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

    void StorageHandler::handleListS3APIKeys(const json& msg, WebSocketSession& session) {
        try {
            unsigned int userId = msg.at("payload").at("userId").get<unsigned int>();
            auto keys = vh::database::APIKeyQueries::listS3APIKeys(userId);

            json response = {
                    {"command", "storage.s3.listApiKeys.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(keys).dump(4)}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed S3 API keys for user ID: " << userId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListS3APIKeys error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.listApiKeys.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleGetS3APIKey(const json& msg, WebSocketSession& session) {
        try {
            unsigned int keyId = msg.at("payload").at("keyId").get<unsigned int>();
            auto key = vh::database::APIKeyQueries::getS3APIKey(keyId);

            json response = {
                    {"command", "storage.s3.getApiKey.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(key)}
            };

            session.send(response);

            std::cout << "[StorageHandler] Fetched S3 API key with ID: " << keyId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleGetS3APIKey error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.getApiKey.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleInitLocalDisk(const json& msg, WebSocketSession& session) {
        try {
            if (vh::database::VaultQueries::localDiskVaultExists())
                throw std::runtime_error("Local disk vault already exists. Only one local disk vault is allowed.");

            json payload = msg.at("payload");
            std::string name = payload.at("name").get<std::string>();
            std::string mountPoint = payload.at("mountPoint").get<std::string>();

            vh::types::LocalDiskVault vault(name, mountPoint);
            vh::database::VaultQueries::addVault(vault);

            json response = {
                    {"command", "storage.local.init.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Mounted local disk storage: " << name << " -> " << mountPoint << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleInitLocalDisk error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.local.init.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleRemoveLocalDiskVault(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("vaultId").get<unsigned int>();

            vh::database::VaultQueries::removeVault(vaultId);

            json response = {
                    {"command", "storage.local.removeVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Removed local disk vault with ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleRemoveLocalDiskVault error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.local.removeVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleGetLocalDiskVault(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("vaultId").get<unsigned int>();
            auto vault = vh::database::VaultQueries::getVault(vaultId);

            json response = {
                    {"command", "storage.local.getVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(vault)}
            };

            session.send(response);

            std::cout << "[StorageHandler] Fetched local disk vault with ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleGetLocalDiskVault error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.local.getVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleInitS3(const json& msg, WebSocketSession& session) {
        try {
            json payload = msg.at("payload");
            std::string name = payload.at("name").get<std::string>();
            unsigned short apiKeyID = payload.at("apiKeyID").get<unsigned short>();
            std::string bucket = payload.at("bucket").get<std::string>();

            vh::database::VaultQueries::addVault(vh::types::S3Vault(name, apiKeyID, bucket));

            json response = {
                    {"command", "storage.s3.init.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Mounted S3 storage: " << name << " -> " << bucket << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleInitS3 error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.init.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleRemoveS3Vault(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("vaultId").get<unsigned int>();

            vh::database::VaultQueries::removeVault(vaultId);

            json response = {
                    {"command", "storage.s3.removeVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Removed S3 vault with ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleRemoveS3Vault error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.removeVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleListS3Vaults(const json& msg, WebSocketSession& session) {
        try {
            auto vaults = vh::database::VaultQueries::listVaults();

            json response = {
                    {"command", "storage.s3.listVaults.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(vaults).dump(4)}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed S3 vaults.\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleListS3Vaults error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.listVaults.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void StorageHandler::handleGetS3Vault(const json& msg, WebSocketSession& session) {
        try {
            unsigned int vaultId = msg.at("payload").at("vaultId").get<unsigned int>();
            auto vault = vh::database::VaultQueries::getVault(vaultId);

            json response = {
                    {"command", "storage.s3.getVault.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(vault)}
            };

            session.send(response);

            std::cout << "[StorageHandler] Fetched S3 vault with ID: " << vaultId << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleGetS3Vault error: " << e.what() << "\n";

            json response = {
                    {"command", "storage.s3.getVault.response"},
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
            unsigned short userID = payload.at("userID").get<unsigned short>();
            unsigned short vaultID = payload.at("vaultID").get<unsigned short>();
            std::string name = payload.at("name").get<std::string>();
            std::string pathPrefix = payload.contains("pathPrefix") ? payload.at("pathPrefix").get<std::string>() : "/";
            unsigned long long quotaBytes = payload.contains("quotaBytes") ? payload.at("quotaBytes").get<unsigned long long>() : 0;

            vh::types::StorageVolume storageVolume(vaultID, name, pathPrefix, quotaBytes);
            vh::database::VaultQueries::addVolume(userID, storageVolume);

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

            vh::database::VaultQueries::removeVolume(volumeId);

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

    void StorageHandler::handleListVolumes(const json& msg, WebSocketSession& session) {
        try {
            unsigned int userId = msg.at("payload").at("userId").get<unsigned int>();
            auto volumes = vh::database::VaultQueries::listVolumes(userId);

            json response = {
                    {"command", "storage.volume.list.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(volumes).dump(4)}
            };

            session.send(response);

            std::cout << "[StorageHandler] Listed volumes for user ID: " << userId << "\n";
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
            auto volume = vh::database::VaultQueries::getVolume(volumeId);

            json response = {
                    {"command", "storage.volume.get.response"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"status", "ok"},
                    {"data", json(volume)}
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
