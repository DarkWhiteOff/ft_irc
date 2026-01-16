#include "Server.hpp"

struct UserInput Server::parseCmd(const std::string &msg)
{
    UserInput input;
    std::istringstream iss(msg);
    std::string token;

    bool cmd_set     = false;
    bool trailing_set = false;

    while (iss >> token)
    {
        if (!cmd_set)
        {
            input.cmd = token;
            cmd_set = true;
        }
        else if (!trailing_set && !token.empty() && token[0] == ':')
        {
            input.trailing = token.substr(1);

            std::string rest;
            std::getline(iss, rest);
            if (!rest.empty() && rest[0] == ' ')
                rest.erase(0, 1);
            if (!rest.empty())
            {
                if (!input.trailing.empty())
                    input.trailing += " ";
                input.trailing += rest;
            }

            trailing_set = true;
            break;
        }
        else
        {
            input.params.push_back(token);
        }
    }

    return input;
}

void Server::handleClientCommand(int client_fd, const std::string& message)
{
    UserInput input = parseCmd(message);

    if (input.cmd == "NICK") {
        if (input.params.empty()) {
            std::string err = ":ft_irc 431 * :No nickname given\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Nickname cannot be empty for client fd "
                      << client_fd << std::endl;
            return ;
        }
        std::string nickname = input.params[0];
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
    else if (input.cmd == "USER")  {
        if (input.params.empty()) {
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":ft_irc 461 " + nick + " USER :Not enough parameters\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The username cannot be empty for client fd "
                      << client_fd << std::endl;
            return ;
        }
        std::string username = input.params[0];
        if (_clientUsernames.find(client_fd) != _clientUsernames.end()) {
            std::string err = ":ft_irc 462 * :You may not reregister\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The username for the client fd " << client_fd << " is already set." << std::endl;
            return ;
        }
        _clientUsernames[client_fd] = username;
        std::cout << "Client fd " << client_fd << " defined the username: " << username << std::endl;
    }
    else if (input.cmd == "JOIN") {
        handleJoinCommand(client_fd, input);
    }
    else if (input.cmd == "PRIVMSG") { // à voir
        if (input.params.empty()) {
            std::string err = ":ft_irc 412 :No text to send\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "PRIVMSG invalid from fd"
                    << client_fd << std::endl;
            return;
        }
        std::string target = input.params[0];
        std::string msg;
        if (!input.trailing.empty())
            msg = input.trailing;
        else
        {
            if (input.params.size() < 2)
            {
                std::string err = ":ft_irc 412 :No text to send\r\n";
                send(client_fd, err.c_str(), err.size(), 0);
                std::cout << "PRIVMSG invalid from fd "
                          << client_fd << std::endl;
                return;
            }
            for (size_t i = 1; i < input.params.size(); ++i)
            {
                if (!msg.empty())
                    msg += " ";
                msg += input.params[i];
            }
        }
        if (target[0] == '#')
            sendChannelPrivmsg(client_fd, target, msg);
        else
            sendUserPrivmsg(client_fd, target, msg);
    }
    else if (input.cmd == "PART") {
        handlePartCommand(client_fd, input);
    }
    else if (input.cmd == "PING") {
        std::string token;
        if (!input.trailing.empty())
            token = input.trailing;
        else if (!input.params.empty())
        {
            token = input.params[0];
            for (size_t i = 1; i < input.params.size(); ++i)
            {
                token += " ";
                token += input.params[i];
            }
        }

        std::string reply = "PONG";
        if (!token.empty())
            reply += " :" + token;
        reply += "\r\n";
        send(client_fd, reply.c_str(), reply.size(), 0);
    }
    else if (input.cmd == "WHO" || input.cmd == "WHOIS") {
        return;
    }
    else if (input.cmd == "KICK" ||
             input.cmd == "INVITE" ||
             input.cmd == "MODE" ||
             input.cmd == "TOPIC") {
        if (input.params.empty())
        {
            std::cout << "Missing channel parameter for command "
                    << input.cmd << " from fd " << client_fd << std::endl;
            return;
        }
        std::string channel = input.params[0];
        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it == _channels.end())
        {
            std::cout << "The channel was not found for the name: "
                    << channel << std::endl;
            return;
        }

        bool isOperator = (it->second.getOperators().find(client_fd) != it->second.getOperators().end());
        if (input.cmd == "TOPIC") {
            bool no_params = (input.params.size() == 1 && input.trailing.empty());
            if ((isOperator || !it->second.getT()) || no_params) {
                handleTopicCommand(client_fd, input);
            } else {
                std::cout << "Client fd " << client_fd
                        << " is not operator and the channel "
                        << channel << " is in mode +t (TOPIC restricted)."
                        << std::endl;
            }
        } else {
            if (!isOperator)
            {
                std::cout << "Client fd " << client_fd
                        << " is not operator on the channel "
                        << channel << " for the command: "
                        << input.cmd << std::endl;
                return;
            }
            if (input.cmd == "MODE")
                handleModeCommand(client_fd, input);
            else
                handleOperatorCommands(client_fd, input);
        }
    }
    else {
        std::cout << "Unknown command received from client fd "
                  << client_fd << ": " << message << std::endl;
    }
}