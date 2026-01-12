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
    std::map<int, std::string>::iterator it = _clientBuffers.begin();
    std::map<int, std::string>::iterator ite = _clientBuffers.end();
    while (it != ite) {
        close(it->first);
        it++;
    }
}

void Server::initServerSocket()
{
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        std::cerr << "Erreur : échec de la création du socket\n";
        return;
    }

    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        close(_serverSocket);
        _serverSocket = -1;
        throw std::runtime_error("Erreur : échec de setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in serverSocketAddr;
    serverSocketAddr.sin_family = AF_INET;
    serverSocketAddr.sin_port = htons(_port);
    inet_pton(AF_INET, "0.0.0.0", &serverSocketAddr.sin_addr);
    if (bind(_serverSocket, (sockaddr*)&serverSocketAddr, sizeof(serverSocketAddr)) < 0) {
        close (_serverSocket);
        _serverSocket = -1;
        // std::cerr << "Erreur : échec du bind (le port est peut-être déjà pris)\n";
        throw std::runtime_error("Erreur : échec du bind (le port est peut-être déjà pris)");
        return ;
    }
    if (listen(_serverSocket, SOMAXCONN) < 0) {
        close (_serverSocket);
        _serverSocket = -1;
        // std::cerr << "Erreur : échec de l'écoute\n";
        throw std::runtime_error("Erreur : échec de l'écoute");
        return ;
    }
    std::cout << "Serveur initialisé sur le port " << _port << std::endl;
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
    fd_set read_fds;
    while (true) {
        read_fds = _masterSet;
        if (select(_maxFd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            std::cerr << "Erreur : échec de select\n";
            continue; // break ?
        }
        for (int fd = 0; fd <= _maxFd; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
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
                    _clientAuthentifieds[clientSocket] = false;
                    std::cout << "Nouvelle connexion acceptée, socket fd: " << clientSocket << std::endl;
                } else {
                    handleClientData(fd, _masterSet, _maxFd);
                }
            }
        }
    }
}