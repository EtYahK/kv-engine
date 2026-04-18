#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace minikv {

class Storage {
public:
    Storage() = default;
    ~Storage() = default;

    Storage(const Storage&)            = delete;
    Storage& operator=(const Storage&) = delete;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    std::size_t size();

private:
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::string> map_;
};

}
