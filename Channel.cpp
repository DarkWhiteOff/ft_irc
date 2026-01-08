#include "Channel.hpp"
#include "iostream"

Channel::Channel(const std::string &name)
    : _name(name), _topic(""), _key(""), _limit(0), i(false), t(false), k(false), l(false) {
    return ;
}

Channel::~Channel() {
    return ;
}

void Channel::setUser(int fd, const std::string &nickname) {
    _users[fd] = nickname;
}

const std::map<int, std::string> &Channel::getUsers() const {
    return _users;
}

void Channel::setOperator(int fd, const std::string &nickname) {
    _operators[fd] = nickname;
}

void Channel::removeUser(int fd) {
    _users.erase(fd);
}

void Channel::removeOperator(int fd) {
    _operators.erase(fd);
}

void Channel::setTopic(const std::string &topic) {
    _topic = topic;
}

void Channel::setI(bool value) {
    i = value;
    if (value == true) {
        std::cout << "Channel " << _name << " set to invite only mode." << std::endl;
    } else {
        std::cout << "Channel " << _name << " unset from invite only mode." << std::endl;
    }
}

void Channel::setT(bool value) {
    t = value;
    if (value == true) {
        std::cout << "Channel " << _name << " set to topic settable by ops only." << std::endl;
    } else {
        std::cout << "Channel " << _name << " unset from topic settable by ops only." << std::endl;
    }
}

void Channel::setK(bool value, const std::string &key) {
    k = value;
    if (value == true && !key.empty()) {
         _key = key;
         std::cout << "Channel " << _name << " set to key protected mode with key: " << _key << std::endl;
    } else if (value == false) {
        _key = "";
        std::cout << "Channel " << _name << " unset from key protected mode." << std::endl;
    }
}

void Channel::setL(bool value, int limit) {
    l = value;
    if (value == true && limit > 0) {
        _limit = limit;
        std::cout << "Channel " << _name << " set to limited user count mode with limit: " << _limit << std::endl;
    } else if (value == false) {
        _limit = 0;
        std::cout << "Channel " << _name << " unset from limited user count mode." << std::endl;
    }
}