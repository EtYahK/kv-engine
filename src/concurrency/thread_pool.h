#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace minikv {

// Fixed-size thread pool with FIFO queue.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Returns false if pool is shutting down.
    bool submit(std::function<void()> task);

    void shutdown();

    std::size_t size() const { return workers_.size(); }

private:
    void worker_loop();

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex              mu_;
    std::condition_variable cv_;
    bool                    stop_ = false;
};
