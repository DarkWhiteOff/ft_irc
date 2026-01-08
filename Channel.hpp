#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>

class Channel {
public:
    Channel(const std::string &name);
    ~Channel();

    void setUser(int fd, const std::string &nickname);
    const std::map<int, std::string> &getUsers() const;

private:
    std::string _name;
    std::map<int, std::string> _users;
};

#endif