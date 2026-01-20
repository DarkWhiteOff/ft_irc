#include "Server.hpp"

void Server::handleJoinCommand(int client_fd, const UserInput &input)
{
    if (input.params.empty() || input.params.size() > 2)
    {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err  = ":ft_irc 461 " + nick
                        + " JOIN :Syntax error (JOIN <#channel> [key])\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    std::string channel = input.params[0];
    std::string key;
    if (input.params.size() >= 2)
        key = input.params[1];

    if (channel.empty() || channel[0] != '#') {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err = ":ft_irc 403 " + nick + " " + channel
                          + " :Invalid channel name. Channel names must start with '#'\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        std::cout << "Invalid channel name received from client fd "
                  << client_fd << ": " << channel << std::endl;
        return ;
    }

    if (_clientNicknames.find(client_fd) == _clientNicknames.end()) {
        std::string err = ":ft_irc 451 * :You have not registered\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        std::cout << "Client fd " << client_fd
                  << " must set a nickname before joining a channel." << std::endl;
        return ;
    }

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it != _channels.end()) {
        bool is_invited = false;
        int  invited_fd = -1;

        std::map<int, std::string>::const_iterator invited_it  =
            it->second.getInvitedUsers().begin();
        std::map<int, std::string>::const_iterator invited_ite =
            it->second.getInvitedUsers().end();

        while (invited_it != invited_ite) {
            if (invited_it->second == _clientNicknames[client_fd]) {
                is_invited = true;
                invited_fd = invited_it->first;
                break;
            }
            invited_it++;
        }

        if (it->second.getL()) {
            if (static_cast<int>(it->second.getUsers().size()) >= it->second.getLimit()) {
                std::string nick = getClientNickOrDefault(client_fd);
                std::string err = ":ft_irc 471 " + nick + " " + channel
                                  + " :Cannot join channel (+l) - channel is full\r\n";
                send(client_fd, err.c_str(), err.size(), 0);
                std::cout << "The channel " << channel << " is full." << std::endl;
                return ;
            }
        }

        if (it->second.getK() && !is_invited) {
            if (key != it->second.getKey()) {
                std::string nick = getClientNickOrDefault(client_fd);
                std::string err = ":ft_irc 475 " + nick + " " + channel
                                  + " :Cannot join channel (+k) - bad key\r\n";
                send(client_fd, err.c_str(), err.size(), 0);
                std::cout << "Incorrect key for the channel "
                          << channel << std::endl;
                return ;
            }
        }

        if (it->second.getI() && !is_invited) {
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":ft_irc 473 " + nick + " " + channel
                              + " :Cannot join channel (+i) - you must be invited\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
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

    sendJoinReplies(client_fd, channel);
    std::cout << "Client fd " << client_fd
              << " joined the channel: " << channel << std::endl;
}

void Server::handlePartCommand(int client_fd, const UserInput &input)
{
    std::string nick = getClientNickOrDefault(client_fd);
    if (input.params.empty())
    {
        std::string err  = ":ft_irc 461 " + nick + " PART :Syntax error (PART <#channel> [:<reason>])\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    std::string channel = input.params[0];
    if (channel.empty() || channel[0] != '#')
    {
        std::string err = ":ft_irc 403 " + nick + " " + channel
                        + " :Invalid channel name. Channel names must start with '#'\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        std::cout << "Invalid channel name received from client fd "
                  << client_fd << " in PART: " << channel << std::endl;
        return;
    }
    std::string reason  = "Leaving";
    if (!input.trailing.empty())
        reason = input.trailing;
    else if (input.params.size() > 1)
    {
        reason.clear();
        for (size_t i = 1; i < input.params.size(); i++)
        {
            if (!reason.empty())
                reason += " ";
            reason += input.params[i];
        }
    }

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it == _channels.end())
    {
        std::string err = ":ft_irc 403 " + nick + " " + channel + " :No such channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    const std::map<int, std::string> &users = it->second.getUsers();
    if (users.find(client_fd) == users.end())
    {
        std::string err = ":ft_irc 442 " + nick + " " + channel + " :You're not on that channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    std::string partMsg = makePrefix(client_fd) + " PART " + channel + " :" + reason + "\r\n";
    std::map<int, std::string>::const_iterator uit = users.begin();
    std::map<int, std::string>::const_iterator uite = users.end();
    while (uit != uite)
    {
        send(uit->first, partMsg.c_str(), partMsg.size(), 0);
        uit++;
    }

    it->second.removeUser(client_fd);
    it->second.removeOperator(client_fd);

    if (it->second.getUsers().empty())
    {
        std::cout << "Channel " << channel << " deleted (no more users)" << std::endl;
        _channels.erase(it);
    }
    else
        promoteNewOperatorIfNeeded(channel, it->second);

    std::cout << "Client fd " << client_fd << " left channel: "
              << channel << " with message: " << reason << std::endl;
}