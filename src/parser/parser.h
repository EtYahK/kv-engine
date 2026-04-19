#pragma once

#include <string>
#include <vector>

namespace minikv {

class Storage;

// Parses commands and returns RESP-formatted responses.
class CommandParser {
public:
    CommandParser(Storage& storage);

    // Returns true to keep connection open.
    bool handle_line(const std::string& line, std::string& out_response);

private:
    static std::vector<std::string> tokenize(const std::string& line);

    Storage& storage_;
};
