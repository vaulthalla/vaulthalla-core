#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace vh::concurrency {

class AsyncService {
public:
    struct Status {
        std::string name;
        bool running = false;
        bool interrupted = false;
    };

    explicit AsyncService(const std::string& serviceName);
    virtual ~AsyncService();

    virtual void start();
    virtual void stop();
    virtual void restart();

    [[nodiscard]] bool isRunning() const { return running_.load(std::memory_order_acquire); }
    [[nodiscard]] virtual Status status() const;
    [[nodiscard]] const std::string& serviceName() const noexcept { return serviceName_; }

protected:
    std::string serviceName_;
    std::atomic<bool> running_{false};
    std::atomic<bool> interruptFlag_{false};
    std::thread worker_;

    virtual void runLoop() = 0;
    // Optional stop hook for derived services to unblock blocking syscalls before join().
    virtual void onStop() {}

    [[nodiscard]] bool shouldStop() const {
        return interruptFlag_.load(std::memory_order_relaxed) ||
               !running_.load(std::memory_order_relaxed);
    }

    // Sleeps for up to `total`, waking periodically to honor interrupts.
    // Returns true if it slept the full duration, false if interrupted early.
    template <class Rep, class Period, class TickRep = long long, class TickPeriod = std::milli>
    bool lazySleep(std::chrono::duration<Rep, Period> total,
                   std::chrono::duration<TickRep, TickPeriod> tick = std::chrono::milliseconds(250)) {
        using namespace std::chrono;

        if (total <= total.zero()) return true;
        if (tick <= tick.zero()) tick = milliseconds(1);

        auto remaining = duration_cast<milliseconds>(total);
        auto step      = duration_cast<milliseconds>(tick);

        while (!shouldStop() && remaining.count() > 0) {
            const auto s = (remaining < step) ? remaining : step;
            std::this_thread::sleep_for(s);
            remaining -= s;
        }

        return remaining.count() <= 0;
    }
};

}
