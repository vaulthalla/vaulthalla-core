#pragma once
#include <string>

namespace vh::database {
    struct DBConfig {
        std::string host = std::getenv("DB_HOST") ? std::getenv("DB_HOST") : "localhost";
        std::string port = std::getenv("DB_PORT") ? std::getenv("DB_PORT") : "5432";
        std::string user = std::getenv("DB_USER") ? std::getenv("DB_USER") : "vaulthalla";
        std::string password = std::getenv("DB_PASSWORD");
        std::string dbname = std::getenv("DB_NAME") ? std::getenv("DB_NAME") : "vaulthalla";

        [[nodiscard]] std::string connectionString() const {
            return "host=" + host +
                   " port=" + port +
                   " user=" + user +
                   " password=" + password +
                   " dbname=" + dbname +
                   " connect_timeout=5";
        }
    };
}
