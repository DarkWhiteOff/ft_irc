#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>

#include <vector>
#include <map>

#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <exception>
#include "Channel.hpp"

#define SERVER_NAME "ft_irc"

class Server {
public:
    Server(int port, const std::string &password);
    ~Server();

    void run();

private:
    int                      _port;
    std::string              _password;
    int                      _serverSocket;
    fd_set                   _masterSet;
    int                      _maxFd;

    std::map<int, std::string> _clientBuffers;
    std::map<int, bool>        _clientAuthentifieds;
    std::map<int, std::string> _clientNicknames;
    std::map<int, std::string> _clientUsernames;
    std::map<int, bool>        _clientRegistered;

    std::map<std::string, Channel> _channels;

    void initServerSocket();
    void initFdSet();

    void handleClientData(int client_fd, fd_set &masterSet, int maxFd);
    void handleClientMessage(int client_fd, const std::string &message);
    void handleOperatorCommands(int client_fd, const std::string &message);
    void handleModeCommand(int client_fd, const std::string &message);
    void handleTopicCommand(int client_fd, const std::string &message);

    void removeClient(int client_fd, fd_set &masterSet);
    void tryRegisterClient(int client_fd);
    void sendReply(int client_fd, const std::string &code,
                   const std::string &nickname, const std::string &message);

    class SocketCreationException : public std::exception
    {
        public :
            virtual const char *what(void) const throw();
    };
    class setsockoptException : public std::exception
    {
        public :
            virtual const char *what(void) const throw();
    };
    class SocketBindException : public std::exception
    {
        public :
            virtual const char *what(void) const throw();
    };
    class SocketListenException : public std::exception
    {
        public :
            virtual const char *what(void) const throw();
    };
};

#endif