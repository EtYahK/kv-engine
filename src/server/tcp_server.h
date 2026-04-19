#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace minikv {

class Storage;
class CommandParser;
class ThreadPool;

// Main thread accepts, thread pool handles each connection (blocking I/O).
class TcpServer {
public:
    TcpServer(Storage& storage, ThreadPool& pool);
    ~TcpServer();

    int run(const std::string& host, uint16_t port);
    void stop();

private:
    void handle_client(int client_fd);

    Storage&          storage_;
    ThreadPool&       pool_;
    std::atomic<bool> running_{false};
    int               listen_fd_ = -1;
};
