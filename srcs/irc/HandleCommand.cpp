#include "Server.hpp"

bool Server::handleClientCommandLOCK(int client_fd, const UserInput &input, fd_set &masterSet)
{
    if (input.cmd == "QUIT")
    {
        if (!input.params.empty() || !input.trailing.empty())
        {
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":ft_irc 461 " + nick
                + " QUIT :Syntax error (QUIT)\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Invalid QUIT from fd " << client_fd << std::endl;
            return true;
        }
        removeClient(client_fd, masterSet);
        return false;
    }
    else if (input.cmd == "PASS") {
        if (input.params.size() != 1 || !input.trailing.empty())
        {
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":ft_irc 461 " + nick
                + " PASS :Syntax error (PASS <password>)\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Invalid PASS from fd " << client_fd << std::endl;
            return true;
        }
        std::string password = input.params[0];
        if (password == _password) {
            _clientAuthentifieds[client_fd] = true;
            std::cout << "Client fd " << client_fd << " authentified successfully." << std::endl;
        } else {
            std::string err = ":ft_irc 464 * :Password incorrect\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Client fd " << client_fd << " failed authentication." << std::endl;
        }
        return true;
    }
    else if (input.cmd == "CAP") {
        return true;
    }
    return true;

}

void Server::handleClientCommand(int client_fd, const std::string &message, const UserInput &input)
{    
    // DEBUG
    std::cout << "[DEBUG] Command: " << input.cmd << std::endl;
    std::cout << "[DEBUG] Params count: " << input.params.size() << std::endl;
    for (size_t i = 0; i < input.params.size(); i++) {
        std::cout << "[DEBUG] Param[" << i << "]: " << input.params[i] << std::endl;
    }
    std::cout << "[DEBUG] Trailing: " << input.trailing << std::endl;
    // DEBUG

    if (input.cmd == "NICK") {
        std::string nick = getClientNickOrDefault(client_fd);
        if (input.params.size() != 1 || !input.trailing.empty()) {
            std::string err = ":ft_irc 461 " + nick
                + " NICK :Syntax error (NICK <nickname>)\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Invalid NICK syntax from fd "
                    << client_fd << std::endl;
            return ;
        }
        std::string nickname = input.params[0];
        if (!nickname.empty() && nickname[0] == '#') {
            std::string err = ":ft_irc 432 " + nick + " " + nickname
                            + " :Erroneous nickname\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Erroneous nickname from fd "
                    << client_fd << ": " << nickname << std::endl;
            return;
        }
        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        while (it != ite) {
            if (it->second == nickname && it->first != client_fd) {
                std::string err = ":ft_irc 433 " + nick + " " + nickname + " :Nickname is already in use\r\n";
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
        std::string nick = getClientNickOrDefault(client_fd);
        if (input.params.empty() || input.params.size() > 3) {
            std::string err = ":ft_irc 461 " + nick
                            + " USER :Syntax error (USER <username>...)\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Invalid USER syntax from fd "
                    << client_fd << std::endl;
            return;
        }
        if (_clientUsernames.find(client_fd) != _clientUsernames.end()) {
            std::string err = ":ft_irc 462 " + nick
                            + " :You may not reregister\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The username for the client fd "
                    << client_fd << " is already set." << std::endl;
            return;
        }
        std::string username = input.params[0];
        _clientUsernames[client_fd] = username;
        std::cout << "Client fd " << client_fd << " defined the username: " << username << std::endl;
    }
    else if (input.cmd == "JOIN") {
        handleJoinCommand(client_fd, input);
    }
    else if (input.cmd == "PRIVMSG") {
        std::string nick = getClientNickOrDefault(client_fd);
        if (input.params.empty()) {
            std::string err = ":ft_irc 461 " + nick
                            + " PRIVMSG :Syntax error (PRIVMSG <target> :<text>)\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Invalid PRIVMSG syntax from fd " << client_fd << std::endl;
            return;
        }
        std::string target = input.params[0];
        std::string msg;
        if (!input.trailing.empty())
            msg = input.trailing;
        else
        {
            if (input.params.size() < 2) {
                std::string err = ":ft_irc 412 " + nick + " :No text to send\r\n";
                send(client_fd, err.c_str(), err.size(), 0);
                std::cout << "PRIVMSG invalid from fd " << client_fd
                        << " (no message text)" << std::endl;
                return;
            }
            for (size_t i = 1; i < input.params.size(); i++)
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
            for (size_t i = 1; i < input.params.size(); i++)
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
        std::string nick = getClientNickOrDefault(client_fd);
        if (input.params.empty())
        {
            std::string syntax;

            if (input.cmd == "KICK")
                syntax = "Syntax error (KICK <#channel> <nick>)";
            else if (input.cmd == "INVITE")
                syntax = "Syntax error (INVITE <#channel> <nick>)";
            else if (input.cmd == "MODE")
                syntax = "Syntax error (MODE <#channel> <modes> [params...])";
            else if (input.cmd == "TOPIC")
                syntax = "Syntax error (TOPIC <#channel> [:topic])";

            std::string err = ":ft_irc 461 " + nick + " " + input.cmd
                            + " :" + syntax + "\r\n";
            send(client_fd, err.c_str(), err.size(), 0);

            std::cout << "Missing channel parameter for command "
                    << input.cmd << " from fd " << client_fd << std::endl;
            return;
        }
        std::string channel = input.params[0];
        if (channel.empty() || channel[0] != '#')
        {
            std::string err = ":ft_irc 403 " + nick + " " + channel
                            + " :Invalid channel name. Channel names must start with '#'\r\n";
            send(client_fd, err.c_str(), err.size(), 0);

            std::cout << "Invalid channel name for command "
                    << input.cmd << " from fd " << client_fd
                    << ": " << channel << std::endl;
            return;
        }
        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it == _channels.end())
        {
            std::string err  = ":ft_irc 403 " + nick + " "
                            + channel + " :No such channel\r\n";
            send(client_fd, err.c_str(), err.size(), 0);

            std::cout << "The channel was not found for the name: "
                    << channel << std::endl;
            return ;
        }

        bool isOperator = (it->second.getOperators().find(client_fd) != it->second.getOperators().end());
        if (input.cmd == "TOPIC") {
            bool no_params = (input.params.size() == 1 && input.trailing.empty());
            if ((isOperator || !it->second.getT()) || no_params) {
                handleTopicCommand(client_fd, input);
            } else {
                std::string err  = ":ft_irc 482 " + nick + " " + channel
                                + " :You're not channel operator\r\n";
                send(client_fd, err.c_str(), err.size(), 0);

                std::cout << "Client fd " << client_fd
                        << " is not operator and the channel "
                        << channel << " is in mode +t (TOPIC restricted)."
                        << std::endl;
            }
        } else {
            if (!isOperator)
            {
                std::string err  = ":ft_irc 482 " + nick + " " + channel
                                + " :You're not channel operator\r\n";
                send(client_fd, err.c_str(), err.size(), 0);

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