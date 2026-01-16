// #ifndef SERVER_HPP
// #define SERVER_HPP

// #include <iostream>
// #include <string>
// #include <unistd.h>
// #include <cstdlib>

// #include <vector>
// #include <map>

// #include <sys/select.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>

// #include <exception>
// #include "Channel.hpp"

// #define SERVER_NAME "ft_irc"

// #include <csignal>
// static volatile sig_atomic_t g_running = 1;

// class Server {
// public:
//     Server(int port, const std::string &password);
//     ~Server();

//     void run();

// private:
//     int                      _port;
//     std::string              _password;
//     int                      _serverSocket;
//     fd_set                   _masterSet;
//     int                      _maxFd;

//     std::map<int, std::string> _clientBuffers;
//     std::map<int, bool>        _clientAuthentifieds;
//     std::map<int, std::string> _clientNicknames;
//     std::map<int, std::string> _clientUsernames;
//     std::map<int, bool>        _clientRegistered;

//     std::map<std::string, Channel> _channels;

//     void initServerSocket();
//     void initFdSet();

//     void handleClientData(int client_fd, fd_set &masterSet, int maxFd);
//     void removeClient(int client_fd, fd_set &masterSet);
//     void tryRegisterClient(int client_fd);

//     void handleClientCommand(int client_fd, const std::string &message);
//     void handleOperatorCommands(int client_fd, const std::string &message);
//     void handleModeCommand(int client_fd, const std::string &message);
//     void handleTopicCommand(int client_fd, const std::string &message);
    
//     void handleJoinCommand(int client_fd, const std::string &message);
//     void handlePartCommand(int client_fd, const std::string &message);


//     std::string getClientNickOrDefault(int client_fd);
//     std::string makePrefix(int client_fd);
//     void sendJoinReplies(int client_fd, const std::string &channel); 
//     void sendNamesReply(int client_fd, const std::string &channel, Channel &chan);
//     void sendTopicReply(int client_fd, const std::string &channel, Channel &chan);
//     void sendChannelPrivmsg(int client_fd, const std::string &target, const std::string &text);
//     void sendUserPrivmsg(int client_fd, const std::string &target, const std::string &text);
//     void updateNickInChannels(int client_fd, const std::string &oldNick, const std::string &newNick);
//     void broadcastNickChange(int client_fd, const std::string &oldNick, const std::string &newNick);

//     class SocketCreationException : public std::exception
//     {
//         public :
//             virtual const char *what(void) const throw();
//     };
//     class setsockoptException : public std::exception
//     {
//         public :
//             virtual const char *what(void) const throw();
//     };
//     class SocketBindException : public std::exception
//     {
//         public :
//             virtual const char *what(void) const throw();
//     };
//     class SocketListenException : public std::exception
//     {
//         public :
//             virtual const char *what(void) const throw();
//     };
// };

// #endif

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

#include <csignal>
#include <sstream>
static volatile sig_atomic_t g_running = 1;

struct UserInput {
    std::string              cmd;
    std::vector<std::string> params;
    std::string              trailing;
};

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
    void removeClient(int client_fd, fd_set &masterSet);
    void tryRegisterClient(int client_fd);

    void handleClientCommand(int client_fd, const std::string &message);
    void handleOperatorCommands(int client_fd, const UserInput &input);
    void handleModeCommand(int client_fd, const UserInput &input);
    void handleTopicCommand(int client_fd, const UserInput &input);
    
    void handleJoinCommand(int client_fd, const UserInput &input);
    void handlePartCommand(int client_fd, const UserInput &input);


    std::string getClientNickOrDefault(int client_fd);
    std::string makePrefix(int client_fd);
    void sendJoinReplies(int client_fd, const std::string &channel); 
    void sendNamesReply(int client_fd, const std::string &channel, Channel &chan);
    void sendTopicReply(int client_fd, const std::string &channel, Channel &chan);
    void sendChannelPrivmsg(int client_fd, const std::string &target, const std::string &text);
    void sendUserPrivmsg(int client_fd, const std::string &target, const std::string &text);
    void updateNickInChannels(int client_fd, const std::string &oldNick, const std::string &newNick);
    void broadcastNickChange(int client_fd, const std::string &oldNick, const std::string &newNick);

    struct UserInput parseCmd(const std::string &msg);

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