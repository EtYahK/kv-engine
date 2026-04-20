#include "parser/parser.h"
#include "storage/storage.h"
#include "utils/aof.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace minikv {

CommandParser::CommandParser(Storage& storage, Aof* aof)
    : storage_(storage), aof_(aof) {}

std::vector<std::string> CommandParser::tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) out.push_back(std::move(tok));
    return out;
}

static std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper((unsigned char)c));
    return s;
}

static std::string bulk(const std::string& s) {
    std::string out = "$";
    out += std::to_string(s.size());
    out += "\r\n";
    out += s;
    out += "\r\n";
    return out;
}

bool CommandParser::handle_line(const std::string& line,
                                std::string& out) {
    auto tokens = tokenize(line);
    if (tokens.empty()) {
        out = "-ERR empty command\r\n";
        return true;
    }
    const std::string cmd = upper(tokens[0]);

    if (cmd == "PING") {
        out = "+PONG\r\n";
        return true;
    }
    if (cmd == "QUIT") {
        out = "+BYE\r\n";
        return false;
    }
    if (cmd == "DBSIZE") {
        out = ":" + std::to_string(storage_.size()) + "\r\n";
        return true;
    }
    if (cmd == "SET") {
        if (tokens.size() != 3 && tokens.size() != 5) {
            out = "-ERR usage: SET key value [EX seconds]\r\n";
            return true;
        }
        std::optional<std::chrono::seconds> ttl;
        if (tokens.size() == 5) {
            if (upper(tokens[3]) != "EX") {
                out = "-ERR expected EX\r\n";
                return true;
            }
            try {
                long long secs = std::stoll(tokens[4]);
                if (secs <= 0) {
                    out = "-ERR EX must be > 0\r\n";
                    return true;
                }
                ttl = std::chrono::seconds(secs);
            } catch (...) {
                out = "-ERR bad EX value\r\n";
                return true;
            }
        }
        storage_.set(tokens[1], tokens[2], ttl);
        if (aof_) aof_->append(line);
        out = "+OK\r\n";
        return true;
    }
    if (cmd == "GET") {
        if (tokens.size() != 2) {
            out = "-ERR usage: GET key\r\n";
            return true;
        }
        auto v = storage_.get(tokens[1]);
        out   = v ? bulk(*v) : std::string("$-1\r\n");
        return true;
    }
    if (cmd == "DEL") {
        if (tokens.size() != 2) {
            out = "-ERR usage: DEL key\r\n";
            return true;
        }
        bool removed = storage_.del(tokens[1]);
        if (removed && aof_) aof_->append(line);
        out = std::string(":") + (removed ? "1" : "0") + "\r\n";
        return true;
    }
    if (cmd == "EXISTS") {
        if (tokens.size() != 2) {
            out = "-ERR usage: EXISTS key\r\n";
            return true;
        }
        out = std::string(":") + (storage_.exists(tokens[1]) ? "1" : "0") + "\r\n";
        return true;
    }
    if (cmd == "TTL") {
        if (tokens.size() != 2) {
            out = "-ERR usage: TTL key\r\n";
            return true;
        }
        out = ":" + std::to_string(storage_.ttl(tokens[1])) + "\r\n";
        return true;
    }

    out = "-ERR unknown command '" + tokens[0] + "'\r\n";
    return true;
}

}
