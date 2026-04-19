#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace minikv {

// shared_mutex: reads (GET) are more common than writes (SET/DEL).
class Storage {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    Storage();
    ~Storage();

    Storage(const Storage&)            = delete;
    Storage& operator=(const Storage&) = delete;

    void set(const std::string& key,
             const std::string& value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    std::optional<std::string> get(const std::string& key);

    bool del(const std::string& key);

    bool exists(const std::string& key);

    // Returns: -2 (missing), -1 (no TTL), or seconds left.
    long long ttl(const std::string& key);

    std::size_t size();

    // Samples random keys to avoid O(N) full scans.
    void start_expiry_thread(std::chrono::milliseconds interval,
                             std::size_t sample_size = 20);
    void stop_expiry_thread();

private:
    struct Entry {
        std::string              value;
        std::optional<TimePoint> expires_at;
    };

    static bool is_expired(const Entry& e, TimePoint now);

    void expiry_loop(std::chrono::milliseconds interval, std::size_t sample_size);

    mutable std::shared_mutex              mu_;
    std::unordered_map<std::string, Entry> map_;

    std::thread             expiry_thread_;
    std::mutex              expiry_mu_;
    std::condition_variable expiry_cv_;
    bool                    expiry_stop_ = false;
};

