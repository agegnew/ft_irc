#include "Message.hpp"
#include "Utils.hpp"

bool Message::parse(const std::string& line, Message& out) {
    out.prefix.clear();
    out.command.clear();
    out.params.clear();
    out.hasTrailing = false;

    std::size_t i = 0;
    const std::size_t n = line.size();

    while (i < n && line[i] == ' ') ++i;
    if (i >= n) return false;

    if (line[i] == ':') {
        ++i;
        const std::size_t start = i;
        while (i < n && line[i] != ' ') ++i;
        out.prefix = line.substr(start, i - start);
        while (i < n && line[i] == ' ') ++i;
        if (i >= n) return false;
    }

    const std::size_t cmdStart = i;
    while (i < n && line[i] != ' ') ++i;
    out.command = Utils::toUpper(line.substr(cmdStart, i - cmdStart));
    if (out.command.empty()) return false;

    // Up to 14 middle params + optional trailing per RFC. We don't enforce the
    // 15-param cap strictly; IRC clients won't push us over it.
    while (i < n) {
        while (i < n && line[i] == ' ') ++i;
        if (i >= n) break;
        if (line[i] == ':') {
            ++i;
            out.params.push_back(line.substr(i));
            out.hasTrailing = true;
            break;
        }
        const std::size_t start = i;
        while (i < n && line[i] != ' ') ++i;
        out.params.push_back(line.substr(start, i - start));
    }

    return true;
}
