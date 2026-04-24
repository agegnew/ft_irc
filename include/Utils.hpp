#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

namespace Utils {

std::string toUpper(const std::string& s);

// RFC 1459 case folding: {}|^ fold to []\~
std::string ircLower(const std::string& s);
bool        ircEqual(const std::string& a, const std::string& b);

std::vector<std::string> split(const std::string& s, char sep);
std::string              join(const std::vector<std::string>& v, char sep);
std::string              trim(const std::string& s);

bool isValidNick(const std::string& n);
bool isValidChannelName(const std::string& n);

std::string intToStr(long n);
bool        strToInt(const std::string& s, long& out);

} // namespace Utils

#endif
