#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>

Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _serverSocket(-1) {
    initServerSocket();
    initFdSet();
}

Server::~Server() {
    return ;
}

void Server::initServerSocket()
{
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        std::cerr << "Erreur : échec de la création du socket\n";
        return;
    }
    sockaddr_in serverSocketAddr;
    serverSocketAddr.sin_family = AF_INET;
    serverSocketAddr.sin_port = htons(_port);
    inet_pton(AF_INET, "0.0.0.0", &serverSocketAddr.sin_addr);
    if (bind(_serverSocket, (sockaddr*)&serverSocketAddr, sizeof(serverSocketAddr)) < 0) {
        std::cerr << "Erreur : échec du bind (le port est peut-être déjà pris)\n";
        return ;
    }
    if (listen(_serverSocket, SOMAXCONN) < 0) {
        std::cerr << "Erreur : échec de l'écoute\n";
        return ;
    }
}

void Server::initFdSet()
{
    FD_ZERO(&_masterSet);
    FD_SET(_serverSocket, &_masterSet);
    _maxFd = _serverSocket;
    std::cout << "Serveur en attente de connexions..." << std::endl;
}

void Server::run() {
    std::cout << "Server running on port " << _port
              << " with password '" << _password << "'" << std::endl;
    while (true) {
        fd_set readFds;
        FD_ZERO(&readFds);
        readFds = _masterSet;
        if (select(_maxFd + 1, &readFds, NULL, NULL, NULL) < 0) {
            std::cerr << "Erreur : échec de select\n";
            continue; // break ?
        }
        for (int fd = 0; fd <= _maxFd; fd++) {
            if (FD_ISSET(fd, &readFds)) {
                if (fd == _serverSocket) {
                    sockaddr_in clientAddr;
                    socklen_t clientLen = sizeof(clientAddr);
                    int clientSocket = accept(_serverSocket, (sockaddr*)&clientAddr, &clientLen);
                    if (clientSocket < 0) {
                        std::cerr << "Erreur : échec de l'acceptation de la connexion\n";
                        continue;
                    }
                    FD_SET(clientSocket, &_masterSet);
                    if (clientSocket > _maxFd) {
                        _maxFd = clientSocket;
                    }
                    std::cout << "Nouvelle connexion acceptée, socket fd: " << clientSocket << std::endl;
                } else {
                    handleClientData(fd, _masterSet, _maxFd);
                }
            }
        }
    }
}

void Server::handleClientData(int fd, fd_set &masterSet, int maxFd)
{
    (void)maxFd;
    char buffer[1024];
    ssize_t bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            std::cout << "Client déconnecté, socket fd: " << fd << std::endl;
        } else {
            std::cerr << "Erreur : échec de la réception des données\n";
        }
        close(fd);
        FD_CLR(fd, &masterSet);
    } else {
        buffer[bytesRead] = '\0';
        std::cout << "Données reçues du client (fd " << fd << "): " << buffer << std::endl;
        // Echo back the received data
        send(fd, buffer, bytesRead, 0);
    }
}