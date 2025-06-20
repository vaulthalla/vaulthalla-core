#pragma once

#include "DBPool.hpp"
#include <functional>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <string>
#include <utility>

namespace vh::database {

class Transactions {
  public:
    static inline std::shared_ptr<DBPool> dbPool_;

    static void init() { dbPool_ = std::make_unique<vh::database::DBPool>(); }

    template <typename Func>
    static auto exec(const std::string& ctx, Func&& func) -> decltype(func(std::declval<pqxx::work&>())) {
        if (!dbPool_) throw std::runtime_error("Transactions not initialized!");
        auto conn = dbPool_->acquire();
        pqxx::work txn(conn->get());
        try {
            if constexpr (std::is_void_v<decltype(func(txn))>) {
                func(txn);
                txn.commit();
            } else {
                auto result = func(txn);
                txn.commit();
                dbPool_->release(std::move(conn));
                return result;
            }
        } catch (...) {
            dbPool_->release(std::move(conn));
            throw;
        }
        dbPool_->release(std::move(conn));
    }
};

} // namespace vh::database
