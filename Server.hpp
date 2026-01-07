#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <sys/select.h>
// #include <poll.h>

class Server {
public:
    Server(int port, const std::string &password);
    ~Server();

    void run();

private:
    int _port;
    std::string _password;
    int _serverSocket;
    fd_set _masterSet;
    int _maxFd;
    // std::vector<struct pollfd> _pollFds;

    void initServerSocket();
    void initFdSet();
    void handleClientData(int fd, fd_set &masterSet, int maxFd);
};

#endif