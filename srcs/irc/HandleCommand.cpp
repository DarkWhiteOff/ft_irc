#include "Server.hpp"

void Server::handleClientCommand(int client_fd, const std::string& message)
{
    if (message.rfind("NICK ", 0) == 0) {
        std::string nickname = message.substr(5);
        if (nickname.empty()) {
            std::string err = ":ft_irc 431 * :No nickname given\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Nickname cannot be empty for client fd "
                      << client_fd << std::endl;
            return ;
        }
        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        while (it != ite) {
            if (it->second == nickname && it->first != client_fd) {
                std::string err = ":ft_irc 433 * " + nickname + " :Nickname is already in use\r\n";
                send(client_fd, err.c_str(), err.size(), 0);
                std::cout << "Nickname " << nickname << " is already used." << std::endl;
                return ;
            }
            it++;
        }

        std::string oldNickname;
        std::map<int, std::string>::iterator itOld = _clientNicknames.find(client_fd);
        if (itOld != _clientNicknames.end())
            oldNickname = itOld->second;
        _clientNicknames[client_fd] = nickname;
        updateNickInChannels(client_fd, oldNickname, nickname);
        broadcastNickChange(client_fd, oldNickname, nickname);
        std::cout << "Client fd " << client_fd << " defined the nickname: " << nickname << std::endl;
    }
    else if (message.rfind("USER ", 0) == 0)  {
        std::string username = message.substr(5);
        if (username.empty()) {
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":ft_irc 461 " + nick + " USER :Not enough parameters\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The username cannot be empty for client fd "
                      << client_fd << std::endl;
            return ;
        }
        if (_clientUsernames.find(client_fd) != _clientUsernames.end()) {
            std::string err = ":ft_irc 462 * :You may not reregister\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The username for the client fd " << client_fd << " is already set." << std::endl;
            return ;
        }
        _clientUsernames[client_fd] = username;
        std::cout << "Client fd " << client_fd << " defined the username: " << username << std::endl;
    }
    else if (message.rfind("JOIN ", 0) == 0) {
        handleJoinCommand(client_fd, message);
    }
    else if (message.rfind("PRIVMSG ", 0) == 0) {
        std::string params = message.substr(8);
        if (params.find(" :") == std::string::npos) {
            std::string err = ":ft_irc 412 :No text to send\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "PRIVMSG invalid from fd"
                    << client_fd << ": " << params << std::endl;
            return;
        }
        std::string target = params.substr(0, params.find(" :"));
        std::string msg   = params.substr(params.find(" :") + 2);
        if (target.empty() || msg.empty()) {
            std::string err = ":ft_irc 412 :No text to send\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "PRIVMSG invalid from fd "
                  << client_fd << ": " << params << std::endl;
            return;
        }
        if (target[0] == '#')
            sendChannelPrivmsg(client_fd, target, msg);
        else
            sendUserPrivmsg(client_fd, target, msg);
    }
    else if (message.rfind("PART ", 0) == 0) {
        handlePartCommand(client_fd, message);
    }
    else if (message.rfind("PING", 0) == 0) {
        std::string params = message.substr(4);
        if (!params.empty() && params[0] == ' ')
            params.erase(0, 1);
        std::string reply = "PONG";
        if (!params.empty())
            reply += " :" + params;
        reply += "\r\n";
        send(client_fd, reply.c_str(), reply.length(), 0);
    }
    else if (message.rfind("WHO ", 0) == 0 || message.rfind("WHOIS ", 0) == 0) {
        return;
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