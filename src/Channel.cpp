#include "Channel.hpp"

Channel::Channel(const std::string& name)
    : _name(name)
    , _topic()
    , _topicSetter()
    , _topicTime(0)
    , _key()
    , _userLimit(0)
    , _members()
    , _operators()
    , _invited()
    , _inviteOnly(false)
    , _topicOpsOnly(true)   // RFC default: +t on; only ops change topic
    , _hasKey(false)
    , _hasLimit(false)
{}

void Channel::removeMember(int fd) {
    _members.erase(fd);
    _operators.erase(fd);
    _invited.erase(fd);
}

void Channel::setTopic(const std::string& t, const std::string& setter, long when) {
    _topic = t;
    _topicSetter = setter;
    _topicTime = when;
}

void Channel::clearTopic() {
    _topic.clear();
    _topicSetter.clear();
    _topicTime = 0;
}

std::string Channel::modeLetters() const {
    std::string out = "+";
    if (_inviteOnly)   out += 'i';
    if (_topicOpsOnly) out += 't';
    if (_hasKey)       out += 'k';
    if (_hasLimit)     out += 'l';
    if (out == "+")    return ""; // no modes set
    return out;
}
