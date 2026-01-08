#include "Channel.hpp"

Channel::Channel(const std::string &name)
    : _name(name) {
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