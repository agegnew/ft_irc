#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>

// Parsed representation of one IRC protocol line:
//   [':' <prefix> ' '] <command> [<params>...] [':' <trailing>]
// The trailing parameter, when present, is stored as the last element of
// `params` and `hasTrailing` is set.
struct Message {
    std::string              prefix;
    std::string              command;    // uppercased by the parser
    std::vector<std::string> params;
    bool                     hasTrailing;

    Message() : hasTrailing(false) {}

    // Parses one logical line (without trailing CRLF). Returns false if the
    // line yields no command at all (empty/whitespace) — callers should then
    // ignore the line.
    static bool parse(const std::string& line, Message& out);
};

#endif
