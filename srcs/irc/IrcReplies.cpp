#include "Server.hpp"

#include <sstream>
std::string Server::getClientNickOrDefault(int client_fd)
{
    std::map<int, std::string>::iterator it = _clientNicknames.find(client_fd);
    if (it != _clientNicknames.end() && !it->second.empty())
        return it->second;
    std::ostringstream oss;
    oss << "user" << client_fd;
    return oss.str();
}

std::string Server::makePrefix(int client_fd)
{
    std::string nick = getClientNickOrDefault(client_fd);
    return ":" + nick + "!ft_irc";
}

void Server::updateNickInChannels(int client_fd, const std::string &oldNick, const std::string &newNick)
{
    (void)oldNick;
    std::map<std::string, Channel>::iterator it = _channels.begin();
    std::map<std::string, Channel>::iterator ite = _channels.end();
    while (it != ite)
    {
        const std::map<int, std::string> &users = it->second.getUsers();
        if (users.find(client_fd) != users.end())
            it->second.setUser(client_fd, newNick);
        const std::map<int, std::string> &ops = it->second.getOperators();
        if (ops.find(client_fd) != ops.end())
            it->second.setOperator(client_fd, newNick);
        it++;
    }
}

void Server::broadcastNickChange(int client_fd, const std::string &oldNick, const std::string &newNick)
{
    if (oldNick.empty() || oldNick == newNick)
        return;

    std::string prefix = ":" + oldNick + "!" + "ft_irc";
    std::string line = prefix + " NICK :" + newNick + "\r\n";

    send(client_fd, line.c_str(), line.size(), 0);

    std::map<std::string, Channel>::iterator it = _channels.begin();
    std::map<std::string, Channel>::iterator ite = _channels.end();
    while (it != ite)
    {
        const std::map<int, std::string> &users = it->second.getUsers();
        if (users.find(client_fd) != users.end())
        {
            std::map<int, std::string>::const_iterator uit = users.begin();
            std::map<int, std::string>::const_iterator uite = users.end();
            while (uit != uite)
            {
                if (uit->first != client_fd)
                    send(uit->first, line.c_str(), line.size(), 0);
                uit++;
            }
        }
        it++;
    }
}

void Server::sendTopicReply(int client_fd, const std::string &channel, Channel &chan)
{ 
    std::string nick = getClientNickOrDefault(client_fd);
    const std::string &topic = chan.getTopic();

    std::string reply;
    if (topic.empty())
        reply = ":ft_irc 331 " + nick + " " + channel + " :No topic is set\r\n";
    else
        reply = ":ft_irc 332 " + nick + " " + channel + " :" + topic + "\r\n";
    send(client_fd, reply.c_str(), reply.size(), 0);
}

void Server::sendNamesReply(int client_fd, const std::string &channel, Channel &chan)
{
    std::string nick = getClientNickOrDefault(client_fd);
    const std::map<int, std::string> &users = chan.getUsers();
    const std::map<int, std::string> &ops = chan.getOperators();

    std::string names = ":ft_irc 353 " + nick + " = " + channel + " :";
    std::map<int, std::string>::const_iterator it = users.begin();
    std::map<int, std::string>::const_iterator ite = users.end();
    while (it != ite)
    {
        if (ops.find(it->first) != ops.end())
            names += "@" + it->second + " ";
        else
            names += it->second + " ";
        ++it;
    }
    names += "\r\n";
    send(client_fd, names.c_str(), names.length(), 0);

    std::string endNames = ":ft_irc 366 " + nick + " " + channel + " :End of /NAMES list.\r\n";
    send(client_fd, endNames.c_str(), endNames.length(), 0);
}

void Server::sendJoinReplies(int client_fd, const std::string& channel) 
{
    std::map<std::string, Channel>::iterator it = _channels.find(channel);

    std::string prefix = makePrefix(client_fd);
    std::string joinLine = prefix + " JOIN :" + channel + "\r\n";

    const std::map<int, std::string> &users = it->second.getUsers();
    std::map<int, std::string>::const_iterator user_it = users.begin();
    std::map<int, std::string>::const_iterator user_ite = users.end();
    while (user_it != user_ite) {
        send(user_it->first, joinLine.c_str(), joinLine.length(), 0);
        user_it++;
    }
    sendTopicReply(client_fd, channel, it->second);
    sendNamesReply(client_fd, channel, it->second);
}


void Server::sendChannelPrivmsg(int client_fd, const std::string &target, const std::string &text)
{
    std::map<std::string, Channel>::iterator it = _channels.find(target);
    if (it == _channels.end()) {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err  = ":ft_irc 403 " + nick + " " + target
                        + " :No such channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);

        std::cout << "The channel was not found for the name: "
                << target << std::endl;
        return;
    }

    const std::map<int, std::string> &users = it->second.getUsers();
    if (users.find(client_fd) == users.end())
    {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err = ":ft_irc 404 " + nick + " " + target + " :Cannot send to channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    std::string prefix = makePrefix(client_fd);
    std::string line = prefix + " PRIVMSG " + target + " :" + text + "\r\n";

    std::map<int, std::string>::const_iterator itu = users.begin();
    std::map<int, std::string>::const_iterator ite = users.end();
    while (itu != ite)
    {
        if (itu->first != client_fd)
            send(itu->first, line.c_str(), line.size(), 0);
        itu++;
    }
}

void Server::sendUserPrivmsg(int client_fd, const std::string &target, const std::string &text)
{
    int target_fd = -1;
    std::map<int, std::string>::iterator it = _clientNicknames.begin();
    std::map<int, std::string>::iterator ite = _clientNicknames.end();
    while (it != ite)
    {
        if (it->second == target)
        {
            target_fd = it->first;
            break;
        }
        it++; 
    }

    std::string nick = getClientNickOrDefault(client_fd);
    if (target_fd == -1)
    {
        std::string err = ":ft_irc 401 " + nick + " " + target + " :No such nick/channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    std::string prefix = makePrefix(client_fd);
    std::string line = prefix + " PRIVMSG " + target + " :" + text + "\r\n";
    send(target_fd, line.c_str(), line.size(), 0);
    std::cout << "Sending private message from " << nick << " to " << target << ": " << text << std::endl;
}
