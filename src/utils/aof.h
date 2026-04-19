#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace minikv {

class Storage;

// Append-only log: flushes on every write (slow but safe).
class Aof {
public:
    explicit Aof(const std::string& path);
    ~Aof();

    Aof(const Aof&)            = delete;
    Aof& operator=(const Aof&) = delete;

    void replay(Storage& storage);

    void append(const std::string& line);

private:
    std::string   path_;
    std::ofstream out_;
    std::mutex    mu_;
};
