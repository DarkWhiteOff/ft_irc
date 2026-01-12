#include "Server.hpp"
#include <iostream>

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