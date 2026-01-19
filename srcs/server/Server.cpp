#include "Server.hpp"

Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _serverSocket(-1), _maxFd(-1) {
    initServerSocket();
    initFdSet();
}

Server::~Server() {
    if (_serverSocket != -1) {
        close(_serverSocket);
    }
    std::map<int, bool>::iterator it = _clientAuthentifieds.begin();
    std::map<int, bool>::iterator ite = _clientAuthentifieds.end();
    while (it != ite) {
        int fd = it->first;
        close(fd);
        it++;
    }
}

void Server::initServerSocket()
{
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        throw SocketCreationException();
    }

    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        close(_serverSocket);
        _serverSocket = -1;
        throw setsockoptException();
    }

    sockaddr_in serverSocketAddr;
    serverSocketAddr.sin_family = AF_INET;
    serverSocketAddr.sin_port = htons(_port);
    inet_pton(AF_INET, "0.0.0.0", &serverSocketAddr.sin_addr);
    if (bind(_serverSocket, (sockaddr*)&serverSocketAddr, sizeof(serverSocketAddr)) < 0) {
        close (_serverSocket);
        _serverSocket = -1;
        throw SocketBindException();
    }
    if (listen(_serverSocket, SOMAXCONN) < 0) {
        close (_serverSocket);
        _serverSocket = -1;
        throw SocketListenException();
    }
    std::cout << "Socket initialized on port " << _port << std::endl;
}

void Server::initFdSet()
{
    FD_ZERO(&_masterSet);
    FD_SET(_serverSocket, &_masterSet);
    _maxFd = _serverSocket;
}

void Server::run() {
    std::cout << "Server running on port " << _port
              << " with password '" << _password << "'" << std::endl;
    fd_set read_fds;
    while (g_running) {
        read_fds = _masterSet;
        if (select(_maxFd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            std::cerr << "Error : Select failure\n";
            break ;
        }
        for (int fd = 0; fd <= _maxFd; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd == _serverSocket) {
                    sockaddr_in clientAddr;
                    socklen_t clientLen = sizeof(clientAddr);
                    int clientSocket = accept(_serverSocket, (sockaddr*)&clientAddr, &clientLen);
                    if (clientSocket < 0) {
                        std::cerr << "Error : failed to accept connection\n";
                        continue;
                    }
                    FD_SET(clientSocket, &_masterSet);
                    if (clientSocket > _maxFd) {
                        _maxFd = clientSocket;
                    }
                    _clientAuthentifieds[clientSocket] = false;
                    _clientRegistered[clientSocket] = false;
                    std::cout << "New connection accepted, socket fd: " << clientSocket << std::endl;
                } else {
                    handleClientData(fd, _masterSet, _maxFd);
                }
            }
        }
    }
}