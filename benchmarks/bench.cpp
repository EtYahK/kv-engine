#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define CLOSESOCK closesocket
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #define CLOSESOCK ::close
  #define INVALID_SOCKET (-1)
#endif

namespace {

struct Args {
    std::string host = "127.0.0.1";
    uint16_t    port = 6380;
    int         threads = 4;
    int         ops     = 10000;
    std::string mode    = "mixed";
};

Args parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto nxt = [&] { return std::string(argv[++i]); };
        if      (k == "--host")    a.host = nxt();
        else if (k == "--port")    a.port = (uint16_t)std::stoi(nxt());
        else if (k == "--threads") a.threads = std::stoi(nxt());
        else if (k == "--ops")     a.ops     = std::stoi(nxt());
        else if (k == "--mode")    a.mode    = nxt();
    }
    return a;
}

int dial(const std::string& host, uint16_t port) {
    int fd = (int)::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) return -1;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
    ::inet_pton(AF_INET, host.c_str(), &a.sin_addr);
    if (::connect(fd, (sockaddr*)&a, sizeof(a)) != 0) {
        CLOSESOCK(fd);
        return -1;
    }
    return fd;
}

bool send_all(int fd, const std::string& s) {
    const char* p = s.data();
    size_t left = s.size();
    while (left) {
#ifdef _WIN32
        int n = ::send(fd, p, (int)left, 0);
#else
        ssize_t n = ::send(fd, p, left, 0);
#endif
        if (n <= 0) return false;
        p += n; left -= (size_t)n;
    }
    return true;
}

bool read_reply(int fd) {
    std::string first;
    char c;
    for (;;) {
#ifdef _WIN32
        int n = ::recv(fd, &c, 1, 0);
#else
        ssize_t n = ::recv(fd, &c, 1, 0);
#endif
        if (n <= 0) return false;
        if (c == '\n') break;
        first.push_back(c);
    }
    if (!first.empty() && first[0] == '$') {
        int len = std::atoi(first.c_str() + 1);
        if (len < 0) return true;
        std::string buf(len + 2, '\0');
        size_t got = 0;
        while (got < buf.size()) {
#ifdef _WIN32
            int n = ::recv(fd, buf.data() + got, (int)(buf.size() - got), 0);
#else
            ssize_t n = ::recv(fd, buf.data() + got, buf.size() - got, 0);
#endif
            if (n <= 0) return false;
            got += (size_t)n;
        }
    }
    return true;
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    WSADATA d; WSAStartup(MAKEWORD(2,2), &d);
#endif
    Args args = parse(argc, argv);

    std::vector<std::thread> workers;
    std::vector<std::vector<double>> latencies(args.threads);
    std::atomic<int> failures{0};

    auto t0 = std::chrono::steady_clock::now();

    for (int t = 0; t < args.threads; ++t) {
        workers.emplace_back([&, t] {
            int fd = dial(args.host, args.port);
            if (fd < 0) { failures++; return; }
            latencies[t].reserve(args.ops);

            for (int i = 0; i < args.ops; ++i) {
                std::string cmd;
                if (args.mode == "set") {
                    cmd = "SET k" + std::to_string(t) + "_" + std::to_string(i)
                        + " v" + std::to_string(i) + "\n";
                } else if (args.mode == "get") {
                    cmd = "GET k" + std::to_string(t) + "_0\n";
                } else {
                    if ((i & 3) == 0)
                        cmd = "SET k" + std::to_string(t) + "_" + std::to_string(i)
                            + " v" + std::to_string(i) + "\n";
                    else
                        cmd = "GET k" + std::to_string(t) + "_" + std::to_string(i - 1) + "\n";
                }

                auto s = std::chrono::steady_clock::now();
                if (!send_all(fd, cmd) || !read_reply(fd)) { failures++; break; }
                auto e = std::chrono::steady_clock::now();
                double us = std::chrono::duration<double, std::micro>(e - s).count();
                latencies[t].push_back(us);
            }
            CLOSESOCK(fd);
        });
    }
    for (auto& w : workers) w.join();

    auto t1 = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();

    std::vector<double> all;
    for (auto& v : latencies) all.insert(all.end(), v.begin(), v.end());
    std::sort(all.begin(), all.end());

    auto pct = [&](double p) {
        if (all.empty()) return 0.0;
        size_t idx = (size_t)((all.size() - 1) * p);
        return all[idx];
    };

    std::cout << "threads:  " << args.threads << "\n";
    std::cout << "ops/thr:  " << args.ops     << "\n";
    std::cout << "total:    " << all.size()   << " ops in "
              << seconds << "s\n";
    std::cout << "QPS:      " << (all.empty() ? 0.0 : all.size() / seconds) << "\n";
    std::cout << "p50 (us): " << pct(0.50) << "\n";
    std::cout << "p95 (us): " << pct(0.95) << "\n";
    std::cout << "p99 (us): " << pct(0.99) << "\n";
    std::cout << "failures: " << failures.load() << "\n";

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
