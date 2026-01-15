#include "Server.hpp" //

void Server::handleClientMessage(int client_fd, const std::string& message)
{
    if (message.rfind("NICK ", 0) == 0) {
        std::string nickname = message.substr(5);
        if (nickname.empty()) {
            std::string err = ":" SERVER_NAME " 431 * :No nickname given\r\n"; //
            send(client_fd, err.c_str(), err.size(), 0); //
            std::cout << "Nickname cannot be empty for client fd "
                      << client_fd << std::endl;
            return ;
        }
        // Vérif doublon //
        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        while (it != ite) {
            if (it->second == nickname && it->first != client_fd) {
                std::string err = ":" SERVER_NAME " 433 * " + nickname + " :Nickname is already in use\r\n"; //
                send(client_fd, err.c_str(), err.size(), 0); //
                std::cout << "Nickname " << nickname << " is already used." << std::endl;
                return ;
            }
            it++;
        }

        std::string oldNickname;
        std::map<int, std::string>::iterator itOld = _clientNicknames.find(client_fd); //
        if (itOld != _clientNicknames.end()) //
            oldNickname = itOld->second; //

        _clientNicknames[client_fd] = nickname;
        updateNickInChannels(client_fd, oldNickname, nickname); //
        broadcastNickChange(client_fd, oldNickname, nickname); //

        std::cout << "Client fd " << client_fd << " defined the nickname: " << nickname << std::endl;
    }
    else if (message.rfind("USER ", 0) == 0)  {
        std::string username = message.substr(5);
        if (username.empty()) {
            std::string nick = getClientNickOrDefault(client_fd); //
            std::string err = ":" SERVER_NAME " 461 " + nick + " USER :Not enough parameters\r\n"; //
            send(client_fd, err.c_str(), err.size(), 0); //
            std::cout << "The username cannot be empty for client fd "
                      << client_fd << std::endl;
            return ;
        }
        if (_clientUsernames.find(client_fd) != _clientUsernames.end()) {
            std::string err = ":" SERVER_NAME " 462 * :You may not reregister\r\n"; //
            send(client_fd, err.c_str(), err.size(), 0); //
            std::cout << "The username for the client fd " << client_fd << " is already set." << std::endl;
            return ;
        }
        _clientUsernames[client_fd] = username;
        std::cout << "Client fd " << client_fd << " defined the username: " << username << std::endl;
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
            std::string nick = getClientNickOrDefault(client_fd); //
            std::string err = ":" SERVER_NAME " 403 " + nick + " " + channel + " :Invalid channel name. Channel names must start with '#'\r\n"; //
            send(client_fd, err.c_str(), err.size(), 0); //
            std::cout << "Invalid channel name received from client fd "
                      << client_fd << ": " << channel << std::endl;
            return ;
        }

        if (_clientNicknames.find(client_fd) == _clientNicknames.end()) {
            std::string err = ":" SERVER_NAME " 451 * :You have not registered\r\n"; //
            send(client_fd, err.c_str(), err.size(), 0); //
            std::cout << "Client fd " << client_fd << " must set a nickname before joining a channel." << std::endl;
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
                    std::string nick = getClientNickOrDefault(client_fd); //
                    std::string err = ":" SERVER_NAME " 471 " + nick + " " + channel + " :Cannot join channel (+l) - channel is full\r\n"; //
                    send(client_fd, err.c_str(), err.size(), 0); //
                    std::cout << "The channel " << channel << " is full." << std::endl;
                    return ;
                }
            }
            if (it->second.getK() && !is_invited) {
                if (key != it->second.getKey()) {
                    std::string nick = getClientNickOrDefault(client_fd); //
                    std::string err = ":" SERVER_NAME " 475 " + nick + " " + channel + " :Cannot join channel (+k) - bad key\r\n"; //
                    send(client_fd, err.c_str(), err.size(), 0); //
                    std::cout << "Incorrect key for the channel " << channel << std::endl;
                    return ;
                }
            }
            if (it->second.getI() && !is_invited) {
                std::string nick = getClientNickOrDefault(client_fd); //
                std::string err = ":" SERVER_NAME " 473 " + nick + " " + channel + " :Cannot join channel (+i) - you must be invited\r\n"; //
                send(client_fd, err.c_str(), err.size(), 0); //
                std::cout << "Client fd " << client_fd
                        << " is not invited to the channel: "
                        << channel << std::endl;
                return;
            }
            if (is_invited && invited_fd != -1) {
                it->second.removeInvitedUser(invited_fd);
            }
            it->second.setUser(client_fd, _clientNicknames[client_fd]);
        } else {
            _channels.insert(std::make_pair(channel, Channel(channel)));
            it = _channels.find(channel);
            it->second.setUser(client_fd, _clientNicknames[client_fd]);
            it->second.setOperator(client_fd, _clientNicknames[client_fd]);
        }
        IrssiJoin(client_fd, channel);
        std::cout << "Client fd " << client_fd << " joined the channel: " << channel << std::endl;
    }
    else if (message.rfind("PRIVMSG ", 0) == 0) {
        size_t pos = 8;
        if (message.size() <= pos)
        {
            std::cout << "PRIVMSG invalid from fd "
                      << client_fd << ": " << message << std::endl;
            return;
        }

        size_t colonPos = message.find(" :", pos);
        if (colonPos == std::string::npos)
        {
            std::string err = ":" SERVER_NAME " 412 :No text to send\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            return;
        }

        std::string target = message.substr(pos, colonPos - pos);
        while (!target.empty() && target[0] == ' ')
            target.erase(0, 1);
        while (!target.empty() && target[target.size() - 1] == ' ')
            target.erase(target.size() - 1);

        std::string text = message.substr(colonPos + 2);
        if (text.empty())
        {
            std::string err = ":" SERVER_NAME " 412 :No text to send\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            return;
        }

        if (target.empty())
        {
            std::cout << "PRIVMSG invalid from fd "
                      << client_fd << ": " << message << std::endl;
            return;
        }

        if (target[0] == '#')
            sendChannelPrivmsg(client_fd, target, text);
        else
            sendPrivatePrivmsg(client_fd, target, text);
    }
    else if (message.rfind("PING", 0) == 0) {
        std::string params = message.substr(4);
        if (!params.empty() && params[0] == ' ')
            params.erase(0, 1);
        std::string reply = "PONG";
        if (!params.empty())
            reply += " :" + params; //
        reply += "\r\n";
        send(client_fd, reply.c_str(), reply.length(), 0);
    }
    else if (message.rfind("PART ", 0) == 0) { //
        handlePartCommand(client_fd, message); //
    } //
    else if (message.rfind("WHO ", 0) == 0) { //
        // Irssi envoie WHO #chan régulièrement, on ignore pour éviter le spam de logs //
        return; //
    } //
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
            std::cout << "The channel was not found for the name: "
                      << channel << std::endl;
            return ;
        }
        bool isOperator = (it->second.getOperators().find(client_fd) != it->second.getOperators().end());
        if (message.rfind("TOPIC ", 0) == 0) {
            if ((isOperator || !it->second.getT()) || no_params) {
                handleTopicCommand(client_fd, message);
            } else {
                std::cout << "Client fd " << client_fd
                        << " is not operator and the channel "
                        << channel << " is in mode +t (TOPIC restricted)."
                        << std::endl;
            }
        } else {
            if (isOperator) {
                handleOperatorCommands(client_fd, message);
            } else {
                std::cout << "Client fd " << client_fd
                    << " is not operator on the channel "
                    << channel << " for the command: "
                    << message << std::endl;
            }
        }
    }
    else {
        std::cout << "Unknown command received from client fd "
                  << client_fd << ": " << message << std::endl;
    }
}
