#ifndef SERVER_HPP
#define SERVER_HPP

#include <csignal>
#include <map>
#include <poll.h>
#include <string>
#include <vector>

class Client;
class Channel;
#ifdef BONUS
class Bot;
#endif

class Server {
public:
    Server(int port, const std::string& password);
    ~Server();

    // Runs the main poll() loop until g_stop is set. Throws std::runtime_error
    // on a fatal I/O error during setup; runtime errors are logged but never
    // crash the server.
    void run();

    // Accessors used by command handlers.
    const std::string& name() const            { return _serverName; }
    const std::string& creationTs() const      { return _creationTs; }
    const std::string& password() const        { return _password; }

    Client*  getClient(int fd);
    Client*  getClientByNick(const std::string& nick);
    // True if any real client OR reserved nick (e.g. the bot) holds this nick.
    // Use this — NOT getClientByNick — for collision checks.
    bool     isNickTaken(const std::string& nick) const;
    Channel* getChannel(const std::string& name);
    Channel* getOrCreateChannel(const std::string& name, int creatorFd);
    void     dropChannel(const std::string& name);

    // Read-only access to the channel registry (used by JOIN 0 and similar
    // commands that need to enumerate the channels a user is in).
    const std::map<std::string, Channel*>& channels() const { return _channels; }

    // Writes raw protocol to a client's send-buffer. No CRLF handling —
    // caller ensures the line already ends in CRLF (enqueue() does this).
    void sendRaw(int fd, const std::string& line);

    // Emit a numeric reply formatted ":server CODE nick <tail>\r\n".
    void sendNumeric(Client& c, const char* code, const std::string& tail);

    // Send `line` to every member of `ch` except `exceptFd`.
    void broadcastToChannel(const Channel& ch,
                            const std::string& line,
                            int exceptFd);

    // Send `line` to every unique user who shares at least one channel with
    // the user identified by `userFd`. The sending user is NOT included.
    // Useful for NICK and QUIT propagation.
    void broadcastToPeers(int userFd, const std::string& line);

    // Register/unregister a nick in the global index. Nick comparison is case
    // insensitive per RFC 1459 (ircLower folding).
    void      indexNick(const std::string& nick, int fd);
    void      unindexNick(const std::string& nick);

    // Schedule `fd` for disconnect at the end of the current poll iteration.
    // Reason is broadcast as a QUIT to channel peers. Safe to call multiple
    // times on the same fd.
    void scheduleDisconnect(int fd, const std::string& reason);

    // Global signal flag — set by the SIGINT/SIGTERM handler.
    static volatile sig_atomic_t g_stop;

#ifdef BONUS
    Bot* bot() { return _bot; }
#endif

private:
    void setupListener();
    void installSignalHandlers();

    void rebuildPollSet();
    void handleReadable(int fd);
    void handleWritable(int fd);
    void acceptNewClient();
    void reapDisconnected();

    // Iterate over a client's recv-buffer and dispatch each complete line.
    void drainClientLines(Client& c);

    int           _port;
    std::string   _password;
    std::string   _serverName;
    std::string   _creationTs;

    int           _listenFd;

    std::vector<struct pollfd>       _pfds;
    std::map<int, Client*>           _clients;
    std::map<std::string, Channel*>  _channels;   // key = ircLower(name)
    std::map<std::string, int>       _nickIndex;  // key = ircLower(nick)

    std::vector<int>                 _toDisconnect;
    std::map<int, std::string>       _quitReasons;

#ifdef BONUS
    Bot*                             _bot;
#endif

    Server(const Server&);
    Server& operator=(const Server&);
};

#endif
