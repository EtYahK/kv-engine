#include "concurrency/thread_pool.h"
#include "server/tcp_server.h"
#include "storage/storage.h"
#include "utils/aof.h"

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

minikv::TcpServer* g_server = nullptr;

void handle_signal(int) {
    if (g_server) g_server->stop();
}

struct Args {
    std::string host      = "127.0.0.1";
    uint16_t    port      = 6380;
    std::size_t threads   = std::max(2u, std::thread::hardware_concurrency());
    std::string aof_path  = "minikv.aof";
    bool        no_persist = false;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if      (k == "--host")        a.host     = next("--host");
        else if (k == "--port")        a.port     = static_cast<uint16_t>(std::stoi(next("--port")));
        else if (k == "--threads")     a.threads  = static_cast<std::size_t>(std::stoi(next("--threads")));
        else if (k == "--aof")         a.aof_path = next("--aof");
        else if (k == "--no-persist")  a.no_persist = true;
        else {
            std::cerr << "unknown arg: " << k << "\n";
            std::exit(2);
        }
    }
    return a;
}

}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    minikv::Storage storage;

    std::unique_ptr<minikv::Aof> aof;
    if (!args.no_persist) {
        aof = std::make_unique<minikv::Aof>(args.aof_path);
        aof->replay(storage);
    }

    storage.start_expiry_thread(std::chrono::milliseconds(1000), 20);

    minikv::ThreadPool pool(args.threads);
    minikv::TcpServer  server(storage, pool, aof.get());
    g_server = &server;

    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    int rc = server.run(args.host, args.port);

    storage.stop_expiry_thread();
    pool.shutdown();
    return rc;
}