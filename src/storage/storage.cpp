#include "storage/storage.h"

namespace minikv {

void Storage::set(const std::string& key, const std::string& value) {
    std::lock_guard lock(mu_);
    map_[key] = value;
}

std::optional<std::string> Storage::get(const std::string& key) {
    std::lock_guard lock(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;
    return it->second;
}

bool Storage::del(const std::string& key) {
    std::lock_guard lock(mu_);
    return map_.erase(key) > 0;
}

bool Storage::exists(const std::string& key) {
    std::lock_guard lock(mu_);
    return map_.find(key) != map_.end();
}

std::size_t Storage::size() {
    std::lock_guard lock(mu_);
    return map_.size();
}

}
