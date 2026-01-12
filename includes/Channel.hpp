#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>

class Channel {
public:
    Channel(const std::string &name);
    ~Channel();

    void setUser(int fd, const std::string &nickname);
    void setOperator(int fd, const std::string &nickname);
    void setInvitedUser(int fd, const std::string &nickname);

    const std::map<int, std::string> &getUsers() const;
    const std::map<int, std::string> &getOperators() const;
    const std::map<int, std::string> &getInvitedUsers() const;

    void removeUser(int fd);
    void removeOperator(int fd);
    void removeInvitedUser(int fd);

    void setTopic(const std::string &topic);
    std::string getTopic() const;

    void setI(bool value);
    void setT(bool value);
    void setK(bool value, const std::string &key = "");
    void setL(bool value, int limit);

    bool getI() const;
    bool getT() const;
    bool getK() const;
    bool getL() const;
    int getLimit() const;
    std::string getKey() const;

private:
    std::string _name;
    std::string _topic;
    std::string _key;
    int         _limit;
    std::map<int, std::string> _users;
    std::map<int, std::string> _operators;
    std::map<int, std::string> _invitedUsers;

    bool i; // invite only
    bool t; // topic settable by ops only
    bool k; // key (password) protected
    bool l; // limited user count
};

#endif