#pragma once
#include <pqxx/connection>
#include "DBConfig.hpp"

namespace vh::database {
    class DBConnection {
    public:
        DBConnection();
        ~DBConnection();

        pqxx::connection& get();

    private:
        std::unique_ptr<pqxx::connection> conn_;
    };
}
