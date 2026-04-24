#include "Client.hpp"

#include <cstddef>

// IRC spec: a message must not exceed 512 bytes including the trailing CRLF.
// We cap the recv-buffer growth at 4x that to tolerate a small amount of pipe-
// lining; anything longer is almost certainly abuse and we truncate.
static const std::size_t kMaxRecvBuf  = 2048;
static const std::size_t kMaxLineSize = 512;

Client::Client(int fd, const std::string& ip)
    : _fd(fd)
    , _ip(ip)
    , _host(ip)
    , _nick()
    , _user()
    , _real()
    , _recvBuf()
    , _sendBuf()
    , _gotPass(false)
    , _gotNick(false)
    , _gotUser(false)
    , _registered(false)
    , _capNegotiating(false)
    , _markedForQuit(false)
    , _overflowed(false)
    , _isBot(false)
    , _quitReason()
{}

std::string Client::prefix() const {
    std::string n = _nick.empty() ? std::string("*") : _nick;
    std::string u = _user.empty() ? std::string("*") : _user;
    std::string h = _host.empty() ? std::string("*") : _host;
    return n + "!" + u + "@" + h;
}

void Client::setNick(const std::string& n) {
    _nick = n;
    _gotNick = true;
}

void Client::markForQuit(const std::string& reason) {
    _markedForQuit = true;
    _quitReason = reason;
}

void Client::appendRecv(const char* data, std::size_t n) {
    _recvBuf.append(data, n);
    if (_recvBuf.size() > kMaxRecvBuf) {
        // Drop anything past the cap; if there's no newline at all, keep only
        // the tail so a well-behaved client can still sync up.
        std::string::size_type nl = _recvBuf.find('\n');
        if (nl == std::string::npos) {
            _recvBuf.erase(0, _recvBuf.size() - kMaxLineSize);
            _overflowed = true;
        } else if (nl >= kMaxLineSize) {
            _recvBuf.erase(0, nl + 1);
            _overflowed = true;
        }
    }
}

bool Client::extractLine(std::string& out) {
    std::string::size_type nl = _recvBuf.find('\n');
    if (nl == std::string::npos) return false;
    std::string::size_type end = nl;
    if (end > 0 && _recvBuf[end - 1] == '\r') --end;
    out.assign(_recvBuf, 0, end);
    _recvBuf.erase(0, nl + 1);
    // Guard against oversize lines (spec: 512 bytes incl CRLF).
    if (out.size() > kMaxLineSize - 2) {
        out.resize(kMaxLineSize - 2);
        _overflowed = true;
    }
    return true;
}

void Client::enqueue(const std::string& line) {
    _sendBuf += line;
    const std::size_t L = _sendBuf.size();
    if (L < 2 || _sendBuf[L - 2] != '\r' || _sendBuf[L - 1] != '\n') {
        // Strip any single trailing \n the caller may have included, then
        // append CRLF canonically.
        if (L >= 1 && _sendBuf[L - 1] == '\n') _sendBuf.erase(L - 1, 1);
        _sendBuf += "\r\n";
    }
}

void Client::drainSend(std::size_t n) {
    if (n >= _sendBuf.size()) _sendBuf.clear();
    else _sendBuf.erase(0, n);
}
