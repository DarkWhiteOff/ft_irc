#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <sys/select.h>
#include "Channel.hpp"
// #include <poll.h>

class Server {
public:
    Server();
    Server(int port, const std::string &password);
    ~Server();

    void run();

private:
    int _port;
    std::string _password;
    int _serverSocket;
    fd_set _masterSet;
    int _maxFd;
    std::map<int, std::string> _clientBuffers;
    std::map<int, std::string> _clientNicknames;
    std::map<int, std::string> _clientUsernames;
    std::map<std::string, Channel> _channels;
    // std::vector<struct pollfd> _pollFds;

    void initServerSocket();
    void initFdSet();
    void handleClientData(int client_fd, fd_set &masterSet, int maxFd);
    void handleClientMessage(int client_fd, const std::string& message);
};

#endif