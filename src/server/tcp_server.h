#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace minikv {

class Storage;

// Basic TCP server: accepts connections and reads commands.
class TcpServer {
public:
    TcpServer(Storage& storage);
    ~TcpServer();

    int run(const std::string& host, uint16_t port);
    void stop();

private:
    void handle_client(int client_fd);

    Storage&          storage_;
    std::atomic<bool> running_{false};
    int               listen_fd_ = -1;
};
