#include "storage/storage.h"

#include <random>
#include <vector>

namespace minikv {

Storage::Storage() = default;

Storage::~Storage() {
    stop_expiry_thread();
}

bool Storage::is_expired(const Entry& e, TimePoint now) {
    return e.expires_at.has_value() && *e.expires_at <= now;
}

void Storage::set(const std::string& key,
                  const std::string& value,
                  std::optional<std::chrono::seconds> ttl) {
    std::unique_lock lock(mu_);
    Entry& e   = map_[key];
    e.value    = value;
    e.expires_at = ttl ? std::optional<TimePoint>{Clock::now() + *ttl}
                       : std::nullopt;
}

std::optional<std::string> Storage::get(const std::string& key) {
    // Fast path: shared lock.
    {
        std::shared_lock lock(mu_);
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        if (!is_expired(it->second, Clock::now())) {
            return it->second.value;
        }
    }
    // Lazy expiry: delete if expired.
    std::unique_lock lock(mu_);
    auto it = map_.find(key);
    if (it != map_.end() && is_expired(it->second, Clock::now())) {
        map_.erase(it);
    }
    return std::nullopt;
}

bool Storage::del(const std::string& key) {
    std::unique_lock lock(mu_);
    return map_.erase(key) > 0;
}

bool Storage::exists(const std::string& key) {
    return get(key).has_value();
}

long long Storage::ttl(const std::string& key) {
    std::shared_lock lock(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return -2;
    if (!it->second.expires_at) return -1;
    auto now = Clock::now();
    if (*it->second.expires_at <= now) return -2;
    auto left = std::chrono::duration_cast<std::chrono::seconds>(
        *it->second.expires_at - now).count();
    return left;
}

std::size_t Storage::size() {
    std::shared_lock lock(mu_);
    return map_.size();
}

void Storage::start_expiry_thread(std::chrono::milliseconds interval,
                                  std::size_t sample_size) {
    if (expiry_thread_.joinable()) return;
    expiry_stop_    = false;
    expiry_thread_ = std::thread([this, interval, sample_size] {
        expiry_loop(interval, sample_size);
    });
}

void Storage::stop_expiry_thread() {
    {
        std::lock_guard<std::mutex> lock(expiry_mu_);
        expiry_stop_ = true;
    }
    expiry_cv_.notify_all();
    if (expiry_thread_.joinable()) expiry_thread_.join();
}

void Storage::expiry_loop(std::chrono::milliseconds interval,
                          std::size_t sample_size) {
    std::mt19937 rng{std::random_device{}()};

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(expiry_mu_);
            if (expiry_cv_.wait_for(lock, interval,
                                    [this] { return expiry_stop_; })) {
                return;
            }
        }

        std::unique_lock lock(mu_);
        if (map_.empty()) continue;

        const std::size_t n = std::min(sample_size, map_.size());
        std::uniform_int_distribution<std::size_t> dist(0, map_.size() - 1);

        auto now = Clock::now();
        for (std::size_t i = 0; i < n; ++i) {
            auto it = map_.begin();
            std::advance(it, dist(rng) % map_.size());
            if (is_expired(it->second, now)) {
                map_.erase(it);
                if (map_.empty()) break;
            }
        }
    }
}

}
