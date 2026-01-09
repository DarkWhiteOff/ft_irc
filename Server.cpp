#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>

#include <cstring>
#include <stdexcept>

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
    sockaddr_in serverSocketAddr;
    serverSocketAddr.sin_family = AF_INET;
    serverSocketAddr.sin_port = htons(_port);
    inet_pton(AF_INET, "0.0.0.0", &serverSocketAddr.sin_addr);
    if (bind(_serverSocket, (sockaddr*)&serverSocketAddr, sizeof(serverSocketAddr)) < 0) {
        close (_serverSocket);
        std::cerr << "Erreur : échec du bind (le port est peut-être déjà pris)\n";
        return ;
    }
    if (listen(_serverSocket, SOMAXCONN) < 0) {
        close (_serverSocket);
        std::cerr << "Erreur : échec de l'écoute\n";
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

void Server::removeClient(int client_fd, fd_set &masterSet)
{
    std::cout << "Client déconnecté, socket fd: " << client_fd << std::endl;

    // Retirer le client de tous les channels
    std::map<std::string, Channel>::iterator ch_it  = _channels.begin();
    std::map<std::string, Channel>::iterator ch_ite = _channels.end();
    while (ch_it != ch_ite) {
        ch_it->second.removeUser(client_fd);
        ch_it->second.removeOperator(client_fd);
        ++ch_it;
    }

    // Nettoyer les maps associées au client
    _clientBuffers.erase(client_fd);
    _clientAuthentifieds.erase(client_fd);
    _clientNicknames.erase(client_fd);
    _clientUsernames.erase(client_fd);

    // Fermer le fd et le retirer du set
    close(client_fd);
    FD_CLR(client_fd, &masterSet);
}

void Server::handleClientData(int client_fd, fd_set &masterSet, int maxFd)
{
    (void)maxFd;
    char buffer[4096];
    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        if (bytes == 0) {
            removeClient(client_fd, masterSet);
        } else {
            std::cerr << "Erreur : échec de la réception des données sur fd "
                      << client_fd << std::endl;
            removeClient(client_fd, masterSet);
        }
        return ;
    } else {
        _clientBuffers[client_fd].append(buffer, bytes);
        size_t eol_pos;
        while ((eol_pos = _clientBuffers[client_fd].find("\r\n")) != std::string::npos) {
            std::string message = _clientBuffers[client_fd].substr(0, eol_pos);
            _clientBuffers[client_fd].erase(0, eol_pos + 2);
            if (message.empty())
                continue;
            if (message.rfind("PASS ", 0) == 0) {
                std::string password = message.substr(5);
                if (password == _password) {
                    _clientAuthentifieds[client_fd] = true;
                    std::cout << "Client fd " << client_fd << " authentifié avec succès." << std::endl;
                } else {
                    std::cout << "Client fd " << client_fd << " a échoué l'authentification." << std::endl;
                    send(client_fd, "Invalid password.\r\n", 19, 0);
                }
            }
            else if (_clientAuthentifieds[client_fd])
                handleClientMessage(client_fd, message);
            else {
                send(client_fd,
                 "You must authenticate first using PASS command.\r\n", 52, 0);
                std::cout << "Client fd " << client_fd
                      << " n'est pas authentifié. Message ignoré: "
                      << message << std::endl;
            }
        }
    }
}

void Server::handleClientMessage(int client_fd, const std::string& message)
{
    if (message.rfind("NICK ", 0) == 0) {
        std::string nickname = message.substr(5);
        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        while (it != ite) {
            if (it->second == nickname && it->first != client_fd) {
                send(client_fd, "Nickname is already in use.\r\n", 30, 0);
                std::cout << "Le pseudo " << nickname << " est déjà utilisé." << std::endl;
                return ;
            }
            it++;
        }
        _clientNicknames[client_fd] = nickname;
        std::cout << "Client fd " << client_fd << " a défini le pseudo: " << nickname << std::endl;
    }
    else if (message.rfind("USER ", 0) == 0)  {
        std::string username = message.substr(5);
        if (_clientUsernames.find(client_fd) != _clientUsernames.end()) {
            send(client_fd, "Username is already set.\r\n", 27, 0);
            std::cout << "Le nom d'utilisateur pour le client fd " << client_fd << " est déjà défini." << std::endl;
            return ;
        }
        std::map<int, std::string>::iterator it = _clientUsernames.begin();
        std::map<int, std::string>::iterator ite = _clientUsernames.end();
        while (it != ite) {
            if (it->second == username && it->first != client_fd) {
                send(client_fd, "Username is already in use.\r\n", 30, 0);
                std::cout << "Le nom d'utilisateur " << username << " est déjà utilisé." << std::endl;
                return ;
            }
            it++;
        }
        _clientUsernames[client_fd] = username;
        std::cout << "Client fd " << client_fd << " a défini le nom d'utilisateur: " << username << std::endl;
    }
    else if (message.rfind("JOIN ", 0) == 0) {
        std::string channel = message.substr(5);
        if (_clientNicknames.find(client_fd) == _clientNicknames.end()) {
            send(client_fd, "You must at least set a nickname before joining a channel.\r\n", 66, 0);
            std::cout << "Client fd " << client_fd << " doit définir un pseudo avant de rejoindre un canal." << std::endl;
            return ;
        }
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
        bool found = false;
        if (target[0] == '#') {
            std::map<std::string, Channel>::iterator it = _channels.begin();
            std::map<std::string, Channel>::iterator ite = _channels.end();
            while (it != ite) {
                if (it->first == target) {
                    found = true;
                    std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
                    std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
                    while (user_it != user_ite) {
                        if (user_it->first != client_fd) {
                            send(user_it->first, msg.c_str(), msg.length(), 0);
                            std::cout << "Envoi du message au client fd " << user_it->first << ": " << msg << std::endl;
                        }
                        user_it++;
                    }
                   break ; 
                }
                it++;
            }
            if (!found) {
                std::cout << "Le canal n'a pas été trouvé pour le nom: "
                          << target << std::endl;
            }
        } else {
            std::map<int, std::string>::iterator it = _clientNicknames.begin();
            std::map<int, std::string>::iterator ite = _clientNicknames.end();
            while (it != ite) {
                if (it->second == target) {
                    found = true;
                    send(it->first, msg.c_str(), msg.length(), 0);
                    std::cout << "Envoi du message au client fd " << it->first << ": " << msg << std::endl;
                    break;
                }
                it++;
            }
            if (!found)
                std::cout << "Le client n'a pas été trouvé pour le pseudo: " << target << std::endl;
        }
    }
    handleOperatorCommands(client_fd, message);
}

void Server::handleOperatorCommands(int client_fd, const std::string& message)
{
    if (message.rfind("KICK ", 0) == 0) {
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
                    send(user_it->first, "You have been kicked from the channel.\r\n", 41, 0);
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
        std::string channel = rest;
        std::string topic = "";
        if (channel.find(' ') != std::string::npos) {
            topic = channel.substr(channel.find(' ') + 1);
            channel = channel.substr(0, channel.find(' '));
        }
        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it == _channels.end()) {
            std::cout << "Le canal n'a pas été trouvé pour le nom: "
                      << channel << std::endl;
            return;
        }
        if (topic.empty()) {
            std::cout << "Le sujet actuel du canal " << channel
                      << " est: " << it->second.getTopic() << std::endl;
            return;
        }
        it->second.setTopic(topic);
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
    if (it == _channels.end()) {
        std::cout << "Le canal n'a pas été trouvé pour le nom: "
                  << channel << std::endl;
        return;
    }
    for (size_t i = 0; i < mode.length(); ++i) {
        if (mode[i] == '+') {
            ++i;
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
                    case 'o': {
                        int fd = std::atoi(mode_args.c_str());
                        std::map<int, std::string>::iterator itNick = _clientNicknames.find(fd);
                        if (itNick != _clientNicknames.end()) {
                            it->second.setOperator(fd, itNick->second);
                        }
                        break;
                    }
                    case 'l':
                        it->second.setL(true, std::atoi(mode_args.c_str()));
                        break;
                    default:
                        break;
                }
                ++i;
            }
            --i;
        } else if (mode[i] == '-') {
            ++i;
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
                    case 'o': {
                        int fd = std::atoi(mode_args.c_str());
                        it->second.removeOperator(fd);
                        break;
                    }
                    case 'l':
                        it->second.setL(false, 0);
                        break;
                    default:
                        break;
                }
                ++i;
            }
            --i;
        }
    }
    std::cout << "Commande MODE reçue du client fd " << client_fd << ": " << message << std::endl;
}