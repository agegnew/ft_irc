#include "Server.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Message.hpp"
#include "Numerics.hpp"
#include "Utils.hpp"

#ifdef BONUS
#  include "Bot.hpp"
#endif

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

volatile sig_atomic_t Server::g_stop = 0;

static void handleSigStop(int) { Server::g_stop = 1; }

Server::Server(int port, const std::string& password)
    : _port(port)
    , _password(password)
    , _serverName("ircserv.42")
    , _creationTs()
    , _listenFd(-1)
    , _pfds()
    , _clients()
    , _channels()
    , _nickIndex()
    , _toDisconnect()
    , _quitReasons()
#ifdef BONUS
    , _bot(0)
#endif
{
    std::time_t now = std::time(0);
    char buf[64];
    std::tm* tm = std::localtime(&now);
    if (tm) std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    else    std::snprintf(buf, sizeof(buf), "%ld", (long)now);
    _creationTs = buf;

    setupListener();
    installSignalHandlers();

#ifdef BONUS
    _bot = new Bot(*this, "ftbot");
    // Reserve the bot's nick so real users can't claim it. fd == -1 marks a
    // virtual (non-socket) client in the nick index.
    _nickIndex[Utils::ircLower(_bot->nick())] = -1;
#endif
}

Server::~Server() {
#ifdef BONUS
    delete _bot;
#endif
    // Close every client fd and free every Client/Channel before exit.
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it) {
        if (it->first >= 0) ::close(it->first);
        delete it->second;
    }
    for (std::map<std::string, Channel*>::iterator it = _channels.begin();
         it != _channels.end(); ++it) {
        delete it->second;
    }
    if (_listenFd >= 0) ::close(_listenFd);
}

void Server::setupListener() {
    _listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0) throw std::runtime_error("socket: " + std::string(std::strerror(errno)));

    int yes = 1;
    if (::setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        int e = errno;
        ::close(_listenFd); _listenFd = -1;
        throw std::runtime_error("setsockopt(SO_REUSEADDR): " + std::string(std::strerror(e)));
    }

    // macOS-safe form: only F_SETFL + O_NONBLOCK allowed by subject.
    if (::fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0) {
        int e = errno;
        ::close(_listenFd); _listenFd = -1;
        throw std::runtime_error("fcntl: " + std::string(std::strerror(e)));
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(static_cast<uint16_t>(_port));

    if (::bind(_listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        int e = errno;
        ::close(_listenFd); _listenFd = -1;
        throw std::runtime_error("bind: " + std::string(std::strerror(e)));
    }
    if (::listen(_listenFd, 128) < 0) {
        int e = errno;
        ::close(_listenFd); _listenFd = -1;
        throw std::runtime_error("listen: " + std::string(std::strerror(e)));
    }

    struct pollfd pfd;
    pfd.fd      = _listenFd;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    _pfds.push_back(pfd);

    std::fprintf(stdout, "[ircserv] listening on port %d\n", _port);
    std::fflush(stdout);
}

void Server::installSignalHandlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSigStop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART so poll() wakes up on signal
    ::sigaction(SIGINT,  &sa, 0);
    ::sigaction(SIGTERM, &sa, 0);

    // Critical: a dead peer closing its end must not kill us via SIGPIPE; we
    // detect EPIPE/ECONNRESET via send()'s return value instead.
    struct sigaction ign;
    std::memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    ::sigaction(SIGPIPE, &ign, 0);
}

void Server::rebuildPollSet() {
    for (std::size_t i = 0; i < _pfds.size(); ++i) {
        int fd = _pfds[i].fd;
        _pfds[i].revents = 0;
        if (fd == _listenFd) {
            _pfds[i].events = POLLIN;
            continue;
        }
        std::map<int, Client*>::iterator it = _clients.find(fd);
        if (it == _clients.end()) {
            _pfds[i].events = 0;
            continue;
        }
        short ev = POLLIN;
        if (!it->second->sendBuf().empty()) ev |= POLLOUT;
        _pfds[i].events = ev;
    }
}

void Server::run() {
    while (!g_stop) {
        try {
            rebuildPollSet();

            int n = ::poll(&_pfds[0], _pfds.size(), -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::fprintf(stderr, "[ircserv] poll: %s\n", std::strerror(errno));
                continue;
            }

            // Snapshot fds into two local vectors before dispatching so we never
            // touch _pfds while reading/writing (handleReadable may schedule
            // disconnects, handleWritable may too).
            std::vector<int> readFds, writeFds, deadFds;
            readFds.reserve(_pfds.size());
            writeFds.reserve(_pfds.size());

            for (std::size_t i = 0; i < _pfds.size(); ++i) {
                short re = _pfds[i].revents;
                if (re & (POLLERR | POLLNVAL)) {
                    deadFds.push_back(_pfds[i].fd);
                    continue;
                }
                if (re & (POLLIN | POLLHUP)) readFds.push_back(_pfds[i].fd);
                if (re & POLLOUT)             writeFds.push_back(_pfds[i].fd);
            }

            for (std::size_t i = 0; i < readFds.size();  ++i) handleReadable(readFds[i]);
            for (std::size_t i = 0; i < writeFds.size(); ++i) handleWritable(writeFds[i]);
            for (std::size_t i = 0; i < deadFds.size();  ++i)
                scheduleDisconnect(deadFds[i], "I/O error");

            reapDisconnected();
        }
        catch (const std::bad_alloc&) {
            // Subject: must not crash on OOM. Log and keep running; the
            // operation that caused the allocation failure is simply dropped.
            std::fprintf(stderr, "[ircserv] out of memory, dropping work\n");
        }
        catch (const std::exception& e) {
            std::fprintf(stderr, "[ircserv] handler error: %s\n", e.what());
        }
        catch (...) {
            // Last-resort safety net: no command handler should throw anything
            // that isn't a std::exception, but keeping the server alive in the
            // face of an unexpected throw beats crashing.
            std::fprintf(stderr, "[ircserv] unknown exception in main loop\n");
        }
    }

    std::fprintf(stdout, "[ircserv] shutting down cleanly\n");
}

void Server::acceptNewClient() {
    struct sockaddr_in csin;
    socklen_t          csinlen = sizeof(csin);
    int cs = ::accept(_listenFd, (struct sockaddr*)&csin, &csinlen);
    if (cs < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        std::fprintf(stderr, "[ircserv] accept: %s\n", std::strerror(errno));
        return;
    }
    if (::fcntl(cs, F_SETFL, O_NONBLOCK) < 0) {
        std::fprintf(stderr, "[ircserv] fcntl(new fd): %s\n", std::strerror(errno));
        ::close(cs);
        return;
    }

    char ipbuf[INET_ADDRSTRLEN];
    const char* ip = ::inet_ntop(AF_INET, &csin.sin_addr, ipbuf, sizeof(ipbuf));
    std::string ipStr(ip ? ip : "?.?.?.?");

    Client* c = new Client(cs, ipStr);
    _clients[cs] = c;

    struct pollfd pfd;
    pfd.fd      = cs;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    _pfds.push_back(pfd);

    std::fprintf(stdout, "[ircserv] +fd=%d from %s\n", cs, ipStr.c_str());
}

void Server::handleReadable(int fd) {
    if (fd == _listenFd) { acceptNewClient(); return; }

    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client& c = *it->second;

    char buf[4096];
    ssize_t r = ::recv(fd, buf, sizeof(buf), 0);
    if (r == 0) { scheduleDisconnect(fd, "Client exited"); return; }
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return;
        scheduleDisconnect(fd, std::strerror(errno));
        return;
    }
    c.appendRecv(buf, static_cast<std::size_t>(r));
    drainClientLines(c);
}

void Server::drainClientLines(Client& c) {
    std::string line;
    while (c.extractLine(line)) {
        if (line.empty()) continue;
        Message m;
        if (!Message::parse(line, m)) continue;
        CommandHandler::dispatch(*this, c, m);
        if (c.markedForQuit()) {
            // Defer the actual close until sendBuf drains, so any reply the
            // command produced (e.g. 464 PASSWDMISMATCH) makes it out the wire
            // before we close the fd. handleWritable will call
            // scheduleDisconnect once the buffer is empty.
            if (c.sendBuf().empty()) scheduleDisconnect(c.fd(), c.quitReason());
            break;
        }
    }
}

void Server::handleWritable(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client& c = *it->second;
    if (c.sendBuf().empty()) return;

    ssize_t w = ::send(fd, c.sendBuf().data(), c.sendBuf().size(), 0);
    if (w < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return;
        scheduleDisconnect(fd, std::strerror(errno));
        return;
    }
    c.drainSend(static_cast<std::size_t>(w));
    if (c.sendBuf().empty() && c.markedForQuit()) scheduleDisconnect(fd, c.quitReason());
}

void Server::scheduleDisconnect(int fd, const std::string& reason) {
    if (fd < 0) return;
    if (_quitReasons.find(fd) == _quitReasons.end()) _quitReasons[fd] = reason;
    for (std::size_t i = 0; i < _toDisconnect.size(); ++i)
        if (_toDisconnect[i] == fd) return;
    _toDisconnect.push_back(fd);
}

void Server::reapDisconnected() {
    for (std::size_t i = 0; i < _toDisconnect.size(); ++i) {
        int fd = _toDisconnect[i];
        std::map<int, Client*>::iterator it = _clients.find(fd);
        if (it == _clients.end()) continue;
        Client* c = it->second;

        const std::string reason = _quitReasons.count(fd)
                                 ? _quitReasons[fd]
                                 : std::string("Client exited");

        // Broadcast QUIT to channel peers — once per peer thanks to
        // broadcastToPeers's internal dedup.
        if (c->isRegistered()) {
            std::string line = ":" + c->prefix() + " QUIT :" + reason + "\r\n";
            broadcastToPeers(fd, line);
        }

        // Remove from every channel; destroy channel if last member leaves.
        std::vector<std::string> toDrop;
        for (std::map<std::string, Channel*>::iterator cit = _channels.begin();
             cit != _channels.end(); ++cit) {
            if (cit->second->hasMember(fd)) {
                cit->second->removeMember(fd);
                if (cit->second->empty()) toDrop.push_back(cit->first);
            }
        }
        for (std::size_t j = 0; j < toDrop.size(); ++j) dropChannel(toDrop[j]);

        if (!c->nick().empty()) unindexNick(c->nick());

        std::fprintf(stdout, "[ircserv] -fd=%d (%s) %s\n",
                     fd, c->nick().c_str(), reason.c_str());

        // Erase from _pfds.
        for (std::size_t j = 0; j < _pfds.size(); ++j) {
            if (_pfds[j].fd == fd) {
                _pfds.erase(_pfds.begin() + j);
                break;
            }
        }

        ::close(fd);
        _clients.erase(fd);
        delete c;
    }
    _toDisconnect.clear();
    _quitReasons.clear();
}

Client* Server::getClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    return it == _clients.end() ? 0 : it->second;
}

Client* Server::getClientByNick(const std::string& nick) {
    std::map<std::string, int>::iterator it = _nickIndex.find(Utils::ircLower(nick));
    if (it == _nickIndex.end()) return 0;
    if (it->second < 0) return 0; // reserved (bot) nick — no underlying Client
    return getClient(it->second);
}

bool Server::isNickTaken(const std::string& nick) const {
    return _nickIndex.find(Utils::ircLower(nick)) != _nickIndex.end();
}

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(Utils::ircLower(name));
    return it == _channels.end() ? 0 : it->second;
}

Channel* Server::getOrCreateChannel(const std::string& name, int creatorFd) {
    Channel* ch = getChannel(name);
    if (ch) return ch;
    ch = new Channel(name);
    if (creatorFd >= 0) {
        ch->addMember(creatorFd);
        ch->addOperator(creatorFd);
    }
    _channels[Utils::ircLower(name)] = ch;
    return ch;
}

void Server::dropChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(Utils::ircLower(name));
    if (it == _channels.end()) return;
    delete it->second;
    _channels.erase(it);
}

void Server::indexNick(const std::string& nick, int fd) {
    _nickIndex[Utils::ircLower(nick)] = fd;
}

void Server::unindexNick(const std::string& nick) {
    std::map<std::string, int>::iterator it = _nickIndex.find(Utils::ircLower(nick));
    if (it != _nickIndex.end() && it->second >= 0) _nickIndex.erase(it);
}

void Server::sendRaw(int fd, const std::string& line) {
    Client* c = getClient(fd);
    if (!c) return;
    c->enqueue(line);
}

void Server::sendNumeric(Client& c, const char* code, const std::string& tail) {
    const std::string target = c.nick().empty() ? "*" : c.nick();
    std::string line = ":" + _serverName + " " + code + " " + target + " " + tail;
    c.enqueue(line);
}

void Server::broadcastToChannel(const Channel& ch,
                                const std::string& line,
                                int exceptFd) {
    // Copy the member set so handlers mutating membership mid-loop don't
    // invalidate iterators.
    std::vector<int> fds(ch.members().begin(), ch.members().end());
    for (std::size_t i = 0; i < fds.size(); ++i) {
        if (fds[i] == exceptFd) continue;
        Client* p = getClient(fds[i]);
        if (p) p->enqueue(line);
    }
}

void Server::broadcastToPeers(int userFd, const std::string& line) {
    std::set<int> seen;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin();
         it != _channels.end(); ++it) {
        Channel* ch = it->second;
        if (!ch->hasMember(userFd)) continue;
        for (std::set<int>::const_iterator mit = ch->members().begin();
             mit != ch->members().end(); ++mit) {
            if (*mit == userFd) continue;
            if (seen.insert(*mit).second) {
                Client* p = getClient(*mit);
                if (p) p->enqueue(line);
            }
        }
    }
}
