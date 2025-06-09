#pragma once

#include "database/DBConnection.hpp"

#include <pqxx/transaction>
#include <functional>
#include <iostream>
#include <string>

namespace vh::database {

    template <typename Func>
    auto runTransaction(const std::string& context, Func&& func) -> decltype(func(std::declval<pqxx::work&>())) {
        auto& conn = DBConnection().get();
        pqxx::work txn(conn);
        try {
            if constexpr (std::is_void_v<decltype(func(txn))>) {
                func(txn);
                txn.commit();
            } else {
                auto result = func(txn);
                txn.commit();
                return result;
            }
        } catch (const pqxx::sql_error& e) {
            std::cerr << "[" << context << "] SQL error: " << e.what() << "\n";
            throw;
        } catch (const std::exception& e) {
            std::cerr << "[" << context << "] Error: " << e.what() << "\n";
            throw;
        }
    }

} // namespace vh::database
