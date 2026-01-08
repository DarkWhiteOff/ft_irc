#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>

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
                    std::cout << "Nouvelle connexion acceptée, socket fd: " << clientSocket << std::endl;
                } else {
                    handleClientData(fd, _masterSet, _maxFd);
                }
            }
        }
    }
}

void Server::handleClientData(int client_fd, fd_set &masterSet, int maxFd)
{
    (void)maxFd;
    char buffer[4096];
    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        if (bytes == 0) {
            std::cout << "Client déconnecté, socket fd: " << client_fd << std::endl;
        } else {
            std::cerr << "Erreur : échec de la réception des données\n";
        }
        close(client_fd);
        FD_CLR(client_fd, &masterSet);
    } else {
        _clientBuffers[client_fd].append(buffer, bytes);
        size_t eol_pos;
        while ((eol_pos = _clientBuffers[client_fd].find("\r\n")) != std::string::npos) {
            std::string message = _clientBuffers[client_fd].substr(0, eol_pos);
            _clientBuffers[client_fd].erase(0, eol_pos + 2);
            if (!message.empty()) {
                handleClientMessage(client_fd, message);
            }
        }
    }
}

void Server::handleClientMessage(int client_fd, const std::string& message)
{
    if (message.rfind("PASS ", 0) == 0) {
        std::string password = message.substr(5);
        if (password == _password) {
            std::cout << "Client fd " << client_fd << " authentifié avec succès." << std::endl;
        } else {
            std::cout << "Client fd " << client_fd << " a échoué l'authentification." << std::endl;
        }
    }
    else if (message.rfind("NICK ", 0) == 0) {
        std::string nickname = message.substr(5);
        _clientNicknames[client_fd] = nickname;
        std::cout << "Client fd " << client_fd << " a défini le pseudo: " << nickname << std::endl;
    }
    else if (message.rfind("USER ", 0) == 0)  {
        std::string username = message.substr(5);
        _clientUsernames[client_fd] = username;
        std::cout << "Client fd " << client_fd << " a défini le nom d'utilisateur: " << username << std::endl;
    }
    else if (message.rfind("JOIN ", 0) == 0) {
        std::string channel = message.substr(5);
        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it == _channels.end()) {
            _channels.insert(std::make_pair(channel, Channel(channel)));
            it = _channels.find(channel);
        }
        it->second.setUser(client_fd, _clientNicknames[client_fd]);
        std::cout << "Client fd " << client_fd << " a rejoint le canal: " << channel << std::endl;
    }
    else if (message.rfind("PRIVMSG ", 0) == 0) {
        std::string rest = message.substr(8);
        std::string target = rest.substr(0, rest.find(' '));
        std::string msg = rest.substr(rest.find(' ') + 1);
        if (target[0] == '#') {
            std::map<std::string, Channel>::iterator it = _channels.begin();
            std::map<std::string, Channel>::iterator ite = _channels.end();
            while (it != ite) {
                if (it->first == target) {
                    std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
                    std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
                    while (user_it != user_ite) {
                        if (user_it->first != client_fd) {
                            send(user_it->first, msg.c_str(), msg.length(), 0);
                            std::cout << "Envoi du message au client fd " << user_it->first << ": " << msg << std::endl;
                        }
                        user_it++;
                    }
                }
                it++;
            }
            std::cout << "Le canal n'a pas été trouvé pour le nom: " << target << std::endl;
        } else {
            std::map<int, std::string>::iterator it = _clientNicknames.begin();
            std::map<int, std::string>::iterator ite = _clientNicknames.end();
            while (it != ite) {
                if (it->second == target) {
                    send(it->first, msg.c_str(), msg.length(), 0);
                    std::cout << "Envoi du message au client fd " << it->first << ": " << msg << std::endl;
                    break;
                }
                it++;
            }
            std::cout << "Le client n'a pas été trouvé pour le pseudo: " << target << std::endl;
        }
    }
    else if (message.rfind("KICK ", 0) == 0) {
        std::string rest = message.substr(5);
        std::string channel = rest.substr(0, rest.find(' '));
        std::string nickname = rest.substr(rest.find(' ') + 1);
        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it != _channels.end()) {
            std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
            std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
            while (user_it != user_ite) {
                if (user_it->second == nickname) {
                    it->second.removeUser(user_it->first);
                    send(user_it->first, "You have been kicked from the channel.\r\n", 39, 0);
                    std::cout << "Client fd " << user_it->first << " a été expulsé du canal: " << channel << std::endl;
                    break;
                }
                user_it++;
            }
        } else {
            std::cout << "Le canal n'a pas été trouvé pour le nom: " << channel << std::endl;
        }
    }
    else if (message.rfind("INVITE ", 0) == 0) {
        std::string rest = message.substr(7);
        std::string nickname = rest.substr(0, rest.find(' '));
        std::string channel = rest.substr(rest.find(' ') + 1);
        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        while (it != ite) {
            if (it->second == nickname) {
                std::string inviteMsg = "You have been invited to join " + channel + "\r\n";
                send(it->first, inviteMsg.c_str(), inviteMsg.length(), 0);
                std::cout << "Client fd " << it->first << " a été invité au canal: " << channel << std::endl;
                break;
            }
            it++;
        }
    }
    else if (message.rfind("TOPIC ", 0) == 0) {
        std::string rest = message.substr(6);
        std::string channel = rest.substr(0, rest.find(' '));
        std::string topic = rest.substr(rest.find(' ') + 1);
        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it != _channels.end()) {
            it->second.setTopic(topic);
        }
        std::cout << "Le sujet du canal " << channel << " a été défini sur: " << topic << std::endl;
    }
    else if (message.rfind("MODE ", 0) == 0) {
        handleModeCommand(client_fd, message);
    }
    else {
        std::cout << "Message reçu du client fd " << client_fd << ": " << message << std::endl;
    }
}

void Server::handleModeCommand(int client_fd, const std::string& message)
{
    std::string rest = message.substr(5);
    std::string channel = rest.substr(0, rest.find(' '));
    std::string mode = rest.substr(rest.find(' ') + 1);
    std::string mode_args = "";
    if (mode.find(' ') != std::string::npos) {
        mode_args = mode.substr(mode.find(' ') + 1);
        mode = mode.substr(0, mode.find(' '));
    }
    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it != _channels.end()) {
        for (size_t i = 0; i < mode.length(); i++) {
            if (mode[i] == '+') {
                i++;
                while (i < mode.length()) {
                    switch (mode[i]) {
                        case 'i':
                            it->second.setI(true);
                            break;
                        case 't':
                            it->second.setT(true);
                            break;
                        case 'k':
                            it->second.setK(true, mode_args);
                            break;
                        case 'o':
                        {
                            it->second.setOperator(atoi(mode_args.c_str()), _clientNicknames[atoi(mode_args.c_str())]);
                            break;
                        }
                        case 'l':
                            it->second.setL(true, atoi(mode_args.c_str()));
                            break;
                        default:
                            break;
                    }
                    i++;
                }
                i--;
            } else if (mode[i] == '-') {
                i++;
                while (i < mode.length()) {
                    switch (mode[i]) {
                        case 'i':
                            it->second.setI(false);
                            break;
                        case 't':
                            it->second.setT(false);
                            break;
                        case 'k':
                            it->second.setK(false, "");
                            break;
                        case 'o':
                        {
                            it->second.removeOperator(atoi(mode_args.c_str()));
                            break;
                        }
                        case 'l':
                            it->second.setL(false, 0);
                            break;
                        default:
                            break;
                    }
                    i++;
                }
                i--;
            }
        }
        std::cout << "Commande MODE reçue du client fd " << client_fd << ": " << message << std::endl;
    }
}