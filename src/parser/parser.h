#pragma once

#include <string>
#include <vector>

namespace minikv {

class Storage;
class Aof;

// Parses commands and returns RESP-formatted responses.
class CommandParser {
public:
    CommandParser(Storage& storage, Aof* aof);

    // Returns true to keep connection open.
    bool handle_line(const std::string& line, std::string& out_response);

private:
    static std::vector<std::string> tokenize(const std::string& line);

    Storage& storage_;
    Aof*     aof_;
};
