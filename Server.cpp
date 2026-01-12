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

void Server::removeClient(int client_fd, fd_set &masterSet)
{
    std::cout << "Client déconnecté, socket fd: " << client_fd << std::endl;

    std::map<std::string, Channel>::iterator ch_it  = _channels.begin();
    std::map<std::string, Channel>::iterator ch_ite = _channels.end();
    while (ch_it != ch_ite) {
        ch_it->second.removeUser(client_fd);
        ch_it->second.removeOperator(client_fd);
        ++ch_it;
    }

    _clientBuffers.erase(client_fd);
    _clientAuthentifieds.erase(client_fd);
    _clientNicknames.erase(client_fd);
    _clientUsernames.erase(client_fd);

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
        if (nickname.empty()) {
            send(client_fd, "Nickname cannot be empty.\r\n", 28, 0);
            std::cout << "Le pseudo ne peut pas être vide pour le client fd "
                      << client_fd << std::endl;
            return ;
        }
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
        if (username.empty()) {
            send(client_fd, "Username cannot be empty.\r\n", 28, 0);
            std::cout << "Le nom d'utilisateur ne peut pas être vide pour le client fd "
                      << client_fd << std::endl;
            return ;
        }
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
        std::string params  = message.substr(5);
        std::string channel;
        std::string key;

        if (params.find(' ') == std::string::npos) {
            channel = params;
        } else {
            channel = params.substr(0, params.find(' '));
            key     = params.substr(params.find(' ') + 1);
        }
        if (channel.empty() || channel[0] != '#') {
            send(client_fd, "Invalid channel name. Channel names must start with '#'.\r\n", 63, 0);
            std::cout << "Nom de canal invalide reçu du client fd "
                      << client_fd << ": " << channel << std::endl;
            return ;
        }

        if (_clientNicknames.find(client_fd) == _clientNicknames.end()) {
            send(client_fd, "You must at least set a nickname before joining a channel.\r\n", 66, 0);
            std::cout << "Client fd " << client_fd << " doit définir un pseudo avant de rejoindre un canal." << std::endl;
            return ;
        }

        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it != _channels.end()) {
            bool is_invited = false;
            int invited_fd = -1;
            std::map<int, std::string>::const_iterator invited_it = it->second.getInvitedUsers().begin();
            std::map<int, std::string>::const_iterator invited_ite = it->second.getInvitedUsers().end();
            while (invited_it != invited_ite) {
                if (invited_it->second == _clientNicknames[client_fd]) {
                    is_invited = true;
                    invited_fd = invited_it->first;
                    break;
                }
                ++invited_it;
            }
            if (it->second.getL()) {
                if (static_cast<int>(it->second.getUsers().size()) >= it->second.getLimit()) {
                    send(client_fd, "Channel is full.\r\n", 18, 0);
                    std::cout << "Le canal " << channel << " est plein." << std::endl;
                    return ;
                }
            }
            if (it->second.getK() && !is_invited) {
                if (key != it->second.getKey()) {
                    send(client_fd, "Incorrect channel key.\r\n", 25, 0);
                    std::cout << "Clé incorrecte pour le canal " << channel << std::endl;
                    return ;
                }
            }
            if (it->second.getI() && !is_invited) {
                send(client_fd, "You are not invited to this channel.\r\n", 38, 0);
                std::cout << "Client fd " << client_fd
                        << " n'est pas invité au canal: "
                        << channel << std::endl;
                return;
            }
            if (is_invited && invited_fd != -1) {
                it->second.removeInvitedUser(client_fd);
            }
            it->second.setUser(client_fd, _clientNicknames[client_fd]);  
        } else {
            _channels.insert(std::make_pair(channel, Channel(channel)));
            it = _channels.find(channel);
            it->second.setUser(client_fd, _clientNicknames[client_fd]);
            it->second.setOperator(client_fd, _clientNicknames[client_fd]);
        }
        std::cout << "Client fd " << client_fd << " a rejoint le canal: " << channel << std::endl;
    }
    else if (message.rfind("PRIVMSG ", 0) == 0) {
        std::string params = message.substr(8);

        if (params.find(' ') == std::string::npos) {
            std::cout << "PRIVMSG invalide de fd"
                    << client_fd << ": " << params << std::endl;
            return;
        }
        std::string target = params.substr(0, params.find(' '));
        std::string msg    = params.substr(params.find(' ') + 1);
        if (target.empty() || msg.empty()) {
            std::cout << "PRIVMSG invalide de fd "
                  << client_fd << ": " << params << std::endl;
            return;
        }
        msg = msg + "\r\n";
        
        if (target[0] == '#') {
            std::map<std::string, Channel>::iterator it = _channels.find(target);
            if (it == _channels.end()) {
                std::cout << "Le canal n'a pas été trouvé pour le nom: "
                        << target << std::endl;
                return;
            }
            std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
            std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
            while (user_it != user_ite) {
                if (user_it->first != client_fd) {
                    send(user_it->first, msg.c_str(), msg.length(), 0);
                    std::cout << "Envoi du message au client fd " << user_it->first << ": " << msg << std::endl;
                }
                user_it++;
            }
        } else {
            bool found = false;
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
    else if (message.rfind("KICK ", 0) == 0 ||
         message.rfind("INVITE ", 0) == 0 ||
         message.rfind("MODE ", 0) == 0 ||
         message.rfind("TOPIC ", 0) == 0) {
        std::string params = message.substr(message.find(' ') + 1);
        std::string channel;

        bool no_params = false;
        if (params.find(' ') == std::string::npos) {
            no_params = true;
            channel = params;
        } else
            channel = params.substr(0, params.find(' '));

        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it == _channels.end()) {
            std::cout << "Le canal n'a pas été trouvé pour le nom: "
                      << channel << std::endl;
            return ;
        }
        bool isOperator = (it->second.getOperators().find(client_fd) != it->second.getOperators().end());
        if (message.rfind("TOPIC ", 0) == 0) {
            if ((isOperator || !it->second.getT()) || no_params) {
                handleTopicCommand(client_fd, message);
            } else {
                std::cout << "Client fd " << client_fd
                        << " n'est pas opérateur et le canal "
                        << channel << " est en mode +t (TOPIC restreint)."
                        << std::endl;
            }
        } else {
            if (isOperator) {
                handleOperatorCommands(client_fd, message);
            } else {
                std::cout << "Client fd " << client_fd
                    << " n'est pas opérateur sur le canal "
                    << channel << " pour la commande: "
                    << message << std::endl;
            }
        }
    }
    else {
        std::cout << "Commande inconnue reçue du client fd "
                  << client_fd << ": " << message << std::endl;
    }
}

void Server::handleOperatorCommands(int client_fd, const std::string& message)
{
    if (message.rfind("KICK ", 0) == 0) {
        std::string params  = message.substr(5);
        if (params.find(' ') == std::string::npos)
            return;
        std::string channel  = params.substr(0, params.find(' '));
        std::string nickname = params.substr(params.find(' ') + 1);

        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it != _channels.end()) {
            std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
            std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
            while (user_it != user_ite) {
                if (user_it->second == nickname) {
                    it->second.removeUser(user_it->first);
                    it->second.removeOperator(user_it->first);
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
        std::string params  = message.substr(7);
        if (params.find(' ') == std::string::npos)
            return;
        std::string channel  = params.substr(0, params.find(' '));
        std::string nickname = params.substr(params.find(' ') + 1);

        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        std::map<std::string, Channel>::iterator it_channel = _channels.find(channel);
        bool found = false;
        if (it_channel != _channels.end()) {
            while (it != ite) {
                if (it->second == nickname) {
                    it_channel->second.setInvitedUser(it->first, nickname);
                    std::string inviteMsg = "You have been invited to join " + channel + "\r\n";
                    send(it->first, inviteMsg.c_str(), inviteMsg.length(), 0);
                    std::cout << "Client fd " << it->first << " a été invité au canal: " << channel << std::endl;
                    found = true;
                    break;
                }
                it++;
            }
        } else {
            std::cout << "Le canal n'a pas été trouvé pour le nom: " << channel << std::endl;
        }
        if (!found) {
            std::cout << "Le client n'a pas été trouvé pour le pseudo: " << nickname << std::endl;
        }
    }
    else if (message.rfind("MODE ", 0) == 0) {
        handleModeCommand(client_fd, message);
    }
}

void Server::handleTopicCommand(int client_fd, const std::string& message)
{
    (void) client_fd;
    if (message.rfind("TOPIC ", 0) != 0)
        return;
    std::string params = message.substr(6);
    std::string channel;
    std::string topic;

    std::string::size_type spacePos = params.find(' ');
    if (spacePos == std::string::npos) {
        channel = params;
    } else {
        channel = params.substr(0, spacePos);
        topic   = params.substr(spacePos + 1);
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

void Server::handleModeCommand(int client_fd, const std::string& message)
{
    (void) client_fd;
    std::string params = message.substr(5);
    std::string channel;
    std::string mode;
    std::string mode_args;
    if (params.find(' ') == std::string::npos)
        return;
    channel = params.substr(0, params.find(' '));
    std::string rest = params.substr(params.find(' ') + 1);
    if (rest.find(' ') == std::string::npos) {
        mode = rest;
    } else {
        mode      = rest.substr(0, rest.find(' '));
        mode_args = rest.substr(rest.find(' ') + 1);
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
                        std::map<int, std::string>::const_iterator it_users = it->second.getUsers().begin();
                        std::map<int, std::string>::const_iterator ite_users = it->second.getUsers().end();
                        bool user_found = false;
                        while (it_users != ite_users) {
                            if (it_users->second == mode_args) {
                                it->second.setOperator(it_users->first, mode_args);
                                user_found = true;
                                break;
                            }
                            ++it_users;
                        }
                        if (!user_found) {
                            std::cout << "L'utilisateur " << mode_args
                                        << " n'est pas dans le canal " << channel
                                        << std::endl;
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
                        std::map<int, std::string>::const_iterator it_ops = it->second.getOperators().begin();
                        std::map<int, std::string>::const_iterator ite_ops = it->second.getOperators().end();
                        bool op_found = false;
                        while (it_ops != ite_ops) {
                            if (it_ops->second == mode_args) {
                                it->second.removeOperator(it_ops->first);
                                op_found = true;
                                break;
                            }
                            ++it_ops;
                        }
                        if (!op_found) {
                            std::cout << "L'opérateur " << mode_args
                                        << " n'est pas dans le canal " << channel
                                        << std::endl;
                        }
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
}