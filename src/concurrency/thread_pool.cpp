#include "concurrency/thread_pool.h"

namespace minikv {

ThreadPool::ThreadPool(std::size_t num_threads) {
    if (num_threads == 0) num_threads = 1;
    workers_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stop_) return false;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
    return true;
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stop_) return;
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        // Swallow exceptions so one bad task doesn't kill the worker.
        try {
            task();
        } catch (...) {
        }
    }
}

}
