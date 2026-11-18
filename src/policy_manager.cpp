#include "policy_manager.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

PolicyManager::PolicyManager(std::vector<std::string> binary_paths)
    : binary_paths_(std::move(binary_paths)),
      worker_(&PolicyManager::worker_loop, this) {}

PolicyManager::~PolicyManager() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
    }
    cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
    stop_current_scheduler();
}

void PolicyManager::request_policy(uint8_t action_id) {
    if (action_id == current_policy_) {
        return;
    }

    current_policy_ = action_id;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pending_policy_ = action_id;
    }
    cv_.notify_one();
}

void PolicyManager::worker_loop() {
    while (true) {
        uint8_t target = NONE;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return stop_ || pending_policy_ != NONE; });
            if (stop_) {
                return;
            }
            target = pending_policy_;
            pending_policy_ = NONE;
        }

        stop_current_scheduler();
        start_scheduler(target);
    }
}

void PolicyManager::stop_current_scheduler() {
    if (child_pid_ <= 0) {
        return;
    }

    if (kill(child_pid_, SIGTERM) == 0) {
        int status = 0;
        for (int i = 0; i < 200; ++i) {
            if (waitpid(child_pid_, &status, WNOHANG) != 0) {
                child_pid_ = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        kill(child_pid_, SIGKILL);
        waitpid(child_pid_, &status, 0);
    }
    child_pid_ = -1;
}

void PolicyManager::start_scheduler(uint8_t action_id) {
    const std::string& bin_path = binary_paths_[action_id];

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[PolicyManager] fork() failed for policy "
                  << static_cast<int>(action_id) << std::endl;
        return;
    }

    if (pid == 0) {
        setpgid(0, 0);
        close_range(3, ~0U, 0);
        char* const argv[] = {const_cast<char*>(bin_path.c_str()), nullptr};
        execv(bin_path.c_str(), argv);
        _exit(127);
    }

    child_pid_ = pid;
}