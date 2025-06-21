#pragma once

#include "DBConnection.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace vh::database {

class DBPool {
  public:
    DBPool(size_t size = 4) {
        for (size_t i = 0; i < size; ++i) {
            pool_.push(std::make_unique<DBConnection>());
        }
    }

    std::unique_ptr<DBConnection> acquire() {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [&]() { return !pool_.empty(); });
        auto conn = std::move(pool_.front());
        pool_.pop();
        return conn;
    }

    void release(std::unique_ptr<DBConnection> conn) {
        std::lock_guard lock(mtx_);
        pool_.push(std::move(conn));
        cv_.notify_one();
    }

  private:
    std::queue<std::unique_ptr<DBConnection>> pool_;
    std::mutex mtx_;
    std::condition_variable cv_;
};

} // namespace vh::database
