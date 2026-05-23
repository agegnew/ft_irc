#include "Utils.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Utils {

std::string toUpper(const std::string& s) {
    std::string out(s);
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] >= 'a' && out[i] <= 'z') out[i] = static_cast<char>(out[i] - 'a' + 'A');
    }
    return out;
}

std::string ircLower(const std::string& s) {
    std::string out(s);
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(out[i]);
        if (ch >= 'A' && ch <= 'Z')       out[i] = static_cast<char>(ch + ('a' - 'A'));
        else if (ch == '[')               out[i] = '{';
        else if (ch == ']')               out[i] = '}';
        else if (ch == '\\')              out[i] = '|';
        else if (ch == '~')               out[i] = '^';
    }
    return out;
}

bool ircEqual(const std::string& a, const std::string& b) {
    return ircLower(a) == ircLower(b);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string::size_type start = 0;
    while (start <= s.size()) {
        std::string::size_type pos = s.find(sep, start);
        if (pos == std::string::npos) {
            if (start < s.size()) out.push_back(s.substr(start));
            else if (start == s.size() && !s.empty() && s[s.size()-1] == sep) out.push_back("");
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::string join(const std::vector<std::string>& v, char sep) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

std::string trim(const std::string& s) {
    std::string::size_type a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r' || s[b-1] == '\n')) --b;
    return s.substr(a, b - a);
}

// RFC 2812 §2.3.1 nickname grammar — first char letter or special; subsequent
// chars letter/digit/special/-. "special" = []\`_^{}|. We cap at 30 (the
// common modern limit used by ngircd / UnrealIRCd) instead of the RFC 1459
// limit of 9 so HexChat users with longer system usernames aren't rejected.
bool isValidNick(const std::string& n) {
    if (n.empty() || n.size() > 30) return false;
    for (std::size_t i = 0; i < n.size(); ++i) {
        char c = n[i];
        bool letter   = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool digit    = (c >= '0' && c <= '9');
        bool special  = (c == '[' || c == ']' || c == '\\' || c == '`'
                      || c == '_' || c == '^' || c == '{' || c == '}'
                      || c == '|');
        if (i == 0) { if (!(letter || special)) return false; }
        else { if (!(letter || digit || special || c == '-')) return false; }
    }
    return true;
}

bool isValidChannelName(const std::string& n) {
    if (n.size() < 2 || n.size() > 50) return false;
    if (n[0] != '#' && n[0] != '&') return false;
    for (std::size_t i = 1; i < n.size(); ++i) {
        char c = n[i];
        if (c == ' ' || c == ',' || c == 7 || c == '\r' || c == '\n' || c == '\0') return false;
    }
    return true;
}

std::string intToStr(long n) {
    char buf[32];
    std::sprintf(buf, "%ld", n);
    return std::string(buf);
}

bool strToInt(const std::string& s, long& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    bool neg = false;
    if (s[0] == '+' || s[0] == '-') { neg = (s[0] == '-'); i = 1; }
    if (i >= s.size()) return false;
    long v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
        if (v > 2147483647L) return false; // keep within int32 for safety
    }
    out = neg ? -v : v;
    return true;
}

} // namespace Utils
