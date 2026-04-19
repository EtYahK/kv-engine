#include "utils/aof.h"
#include "parser/parser.h"
#include "storage/storage.h"

#include <iostream>

namespace minikv {

Aof::Aof(const std::string& path) : path_(path) {}

Aof::~Aof() {
    if (out_.is_open()) out_.close();
}

void Aof::replay(Storage& storage) {
    std::ifstream in(path_);
    if (!in) {
        out_.open(path_, std::ios::app | std::ios::binary);
        return;
    }

    CommandParser parser(storage, nullptr);
    std::string line;
    std::size_t replayed = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::string resp;
        parser.handle_line(line, resp);
        ++replayed;
    }
    in.close();

    if (replayed > 0) {
        std::cerr << "[aof] replayed " << replayed << " commands from "
                  << path_ << "\n";
    }

    out_.open(path_, std::ios::app | std::ios::binary);
}

void Aof::append(const std::string& line) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!out_.is_open()) return;
    out_ << line << '\n';
    out_.flush();
}

}
