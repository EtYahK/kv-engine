#include "server/tcp_server.h"
#include "concurrency/thread_pool.h"
#include "parser/parser.h"
#include "storage/storage.h"

#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socklen_t = int;
  #define CLOSESOCK closesocket
  #define SOCK_ERR  WSAGetLastError()
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #include <cerrno>
  #define CLOSESOCK ::close
  #define SOCK_ERR  errno
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
#endif

namespace minikv {

namespace {

#ifdef _WIN32
struct WsaInit {
    WsaInit()  { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
};
#endif

bool read_line(int fd, std::string& out) {
    out.clear();
    char c;
    for (;;) {
#ifdef _WIN32
        int n = ::recv(fd, &c, 1, 0);
#else
        ssize_t n = ::recv(fd, &c, 1, 0);
#endif
        if (n <= 0) return false;
        if (c == '\n') {
            if (!out.empty() && out.back() == '\r') out.pop_back();
            return true;
        }
        out.push_back(c);
        if (out.size() > 64 * 1024) return false;
    }
}

bool write_all(int fd, const std::string& data) {
    const char* p     = data.data();
    std::size_t left  = data.size();
    while (left > 0) {
#ifdef _WIN32
        int n = ::send(fd, p, static_cast<int>(left), 0);
#else
        ssize_t n = ::send(fd, p, left, 0);
#endif
        if (n <= 0) return false;
        p    += n;
        left -= static_cast<std::size_t>(n);
    }
    return true;
}

}

TcpServer::TcpServer(Storage& storage, ThreadPool& pool)
    : storage_(storage), pool_(pool) {}

TcpServer::~TcpServer() {
    stop();
}

int TcpServer::run(const std::string& host, uint16_t port) {
#ifdef _WIN32
    static WsaInit wsa;
#endif

    listen_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (listen_fd_ == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << SOCK_ERR << "\n";
        return 1;
    }

    int yes = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "bad host: " << host << "\n";
        return 1;
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << SOCK_ERR << "\n";
        return 1;
    }
    if (::listen(listen_fd_, 128) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << SOCK_ERR << "\n";
        return 1;
    }

    running_ = true;
    std::cerr << "[minikv] listening on " << host << ":" << port << "\n";

    while (running_.load()) {
        sockaddr_in client{};
        socklen_t   len = sizeof(client);
        int client_fd   = static_cast<int>(
            ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client), &len));
        if (client_fd == INVALID_SOCKET) {
            if (!running_.load()) break;
            continue;
        }
        bool ok = pool_.submit([this, client_fd] { handle_client(client_fd); });
        if (!ok) CLOSESOCK(client_fd);
    }

    if (listen_fd_ != INVALID_SOCKET) {
        CLOSESOCK(listen_fd_);
        listen_fd_ = -1;
    }
    return 0;
}

void TcpServer::stop() {
    running_ = false;
    if (listen_fd_ != INVALID_SOCKET) {
        CLOSESOCK(listen_fd_);
        listen_fd_ = -1;
    }
}

void TcpServer::handle_client(int client_fd) {
    CommandParser parser(storage_);
    std::string   line;
    std::string   response;

    while (read_line(client_fd, line)) {
        if (line.empty()) continue;
        bool keep = parser.handle_line(line, response);
        if (!write_all(client_fd, response)) break;
        if (!keep) break;
    }
    CLOSESOCK(client_fd);
}

}
