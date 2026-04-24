#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <set>
#include <string>

class Channel {
public:
    explicit Channel(const std::string& name);

    const std::string& name() const         { return _name; }
    const std::string& topic() const        { return _topic; }
    const std::string& topicSetter() const  { return _topicSetter; }
    long               topicTime() const    { return _topicTime; }
    const std::string& key() const          { return _key; }
    std::size_t        userLimit() const    { return _userLimit; }

    bool inviteOnly() const   { return _inviteOnly; }
    bool topicOpsOnly() const { return _topicOpsOnly; }
    bool hasKey() const       { return _hasKey; }
    bool hasLimit() const     { return _hasLimit; }

    const std::set<int>& members() const   { return _members; }
    const std::set<int>& operators() const { return _operators; }

    bool hasMember(int fd) const  { return _members.find(fd)   != _members.end(); }
    bool isOperator(int fd) const { return _operators.find(fd) != _operators.end(); }
    bool isInvited(int fd) const  { return _invited.find(fd)   != _invited.end(); }
    bool empty() const            { return _members.empty(); }

    void addMember(int fd)    { _members.insert(fd); }
    void removeMember(int fd);

    void addOperator(int fd)    { _operators.insert(fd); }
    void removeOperator(int fd) { _operators.erase(fd); }

    void addInvite(int fd)    { _invited.insert(fd); }
    void removeInvite(int fd) { _invited.erase(fd); }

    void setTopic(const std::string& t, const std::string& setter, long when);
    void clearTopic();

    void setInviteOnly(bool v)   { _inviteOnly = v; }
    void setTopicOpsOnly(bool v) { _topicOpsOnly = v; }

    void setKey(const std::string& k) { _key = k;  _hasKey = true;  }
    void clearKey()                   { _key.clear(); _hasKey = false; }

    void setUserLimit(std::size_t n)  { _userLimit = n; _hasLimit = true; }
    void clearUserLimit()              { _userLimit = 0; _hasLimit = false; }

    // "+itk" etc (no args). Used for RPL_CHANNELMODEIS.
    std::string modeLetters() const;

private:
    std::string   _name;
    std::string   _topic;
    std::string   _topicSetter;   // "nick!user@host" of whoever set it
    long          _topicTime;
    std::string   _key;
    std::size_t   _userLimit;

    std::set<int> _members;
    std::set<int> _operators;
    std::set<int> _invited;

    bool _inviteOnly;
    bool _topicOpsOnly;
    bool _hasKey;
    bool _hasLimit;

    // non-copyable
    Channel(const Channel&);
    Channel& operator=(const Channel&);
};

#endif
