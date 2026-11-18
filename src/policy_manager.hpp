#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PolicyManager {
public:
    explicit PolicyManager(std::vector<std::string> binary_paths);
    ~PolicyManager();

    PolicyManager(const PolicyManager&) = delete;
    PolicyManager& operator=(const PolicyManager&) = delete;

    // Single-threaded caller only; returns immediately
    void request_policy(uint8_t action_id);

private:
    void worker_loop();
    void stop_current_scheduler();
    void start_scheduler(uint8_t action_id);

    static constexpr uint8_t NONE = 255;

    std::vector<std::string> binary_paths_;
    uint8_t current_policy_{NONE};

    std::mutex mtx_;
    std::condition_variable cv_;
    uint8_t pending_policy_{NONE};
    bool stop_{false};

    pid_t child_pid_{-1};
    std::thread worker_;
};