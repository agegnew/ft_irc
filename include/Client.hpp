#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client {
public:
    Client(int fd, const std::string& ip);

    int                fd() const        { return _fd; }
    const std::string& ip() const        { return _ip; }
    const std::string& host() const      { return _host; }
    const std::string& nick() const      { return _nick; }
    const std::string& user() const      { return _user; }
    const std::string& real() const      { return _real; }
    bool               isBot() const     { return _isBot; }

    bool gotPass() const       { return _gotPass; }
    bool gotNick() const       { return _gotNick; }
    bool gotUser() const       { return _gotUser; }
    bool isRegistered() const  { return _registered; }
    bool capNegotiating() const{ return _capNegotiating; }
    bool markedForQuit() const { return _markedForQuit; }

    const std::string& quitReason() const { return _quitReason; }
    const std::string& sendBuf() const    { return _sendBuf; }
    std::string&       sendBuf()          { return _sendBuf; }

    std::string prefix() const;

    void setHost(const std::string& h)    { _host = h; }
    void setNick(const std::string& n);
    void setUser(const std::string& u,
                 const std::string& r)    { _user = u; _real = r; _gotUser = true; }
    void setGotPass(bool v)               { _gotPass = v; }
    void markRegistered()                 { _registered = true; }
    void setCapNegotiating(bool v)        { _capNegotiating = v; }
    void markForQuit(const std::string& reason);
    void setBot(bool v)                   { _isBot = v; }

    // Append received bytes to the recv-buffer.
    void appendRecv(const char* data, std::size_t n);

    // Pop the first complete CRLF- (or LF-) terminated line from the recv-buffer.
    // Returns true + fills `out` if a line was extracted (trailing \r stripped).
    // Returns false if no complete line is present yet.
    // If the buffer grows past 512 bytes without a terminator, the head is
    // truncated to keep memory bounded — the caller can check `overflowed()`.
    bool extractLine(std::string& out);

    bool overflowed() const { return _overflowed; }
    void clearOverflow()    { _overflowed = false; }

    // Append a line to the outbound buffer. If `line` does not end in CRLF,
    // CRLF is added.
    void enqueue(const std::string& line);

    // Mark that the first `n` bytes of the send-buffer have been flushed.
    void drainSend(std::size_t n);

private:
    int         _fd;
    std::string _ip;
    std::string _host;
    std::string _nick;
    std::string _user;
    std::string _real;

    std::string _recvBuf;
    std::string _sendBuf;

    bool _gotPass;
    bool _gotNick;
    bool _gotUser;
    bool _registered;
    bool _capNegotiating;
    bool _markedForQuit;
    bool _overflowed;
    bool _isBot;

    std::string _quitReason;

    Client(const Client&);
    Client& operator=(const Client&);
};

#endif
