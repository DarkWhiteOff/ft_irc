#include "Server.hpp"

void Server::handleTopicCommand(int client_fd, const UserInput &input)
{
    std::string channel = input.params[0];
    std::string topic;
    if (!input.trailing.empty())
        topic = input.trailing;
    else if (input.params.size() > 1)
    {
        for (size_t i = 1; i < input.params.size(); i++)
        {
            if (!topic.empty())
                topic += " ";
            topic += input.params[i];
        }
    }

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    const std::map<int, std::string> &users = it->second.getUsers();
    if (users.find(client_fd) == users.end())
    {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err = ":ft_irc 442 " + nick + " " + channel + " :You're not on that channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }
    if (topic.empty()) {
        sendTopicReply(client_fd, channel, it->second);
        return;
    }
    it->second.setTopic(topic);
    std::string topicMsg = makePrefix(client_fd) + " TOPIC " + channel + " :" + topic + "\r\n";
    std::map<int, std::string>::const_iterator uit = users.begin();
    std::map<int, std::string>::const_iterator uite = users.end();
    while (uit != uite)
    {
        send(uit->first, topicMsg.c_str(), topicMsg.size(), 0);
        uit++;
    }
    std::cout << "The topic of the channel " << channel << " has been set to: " << topic << std::endl;
}

void Server::handleOperatorCommands(int client_fd, const UserInput &input)
{
    if (input.cmd == "KICK") {
        std::string nick = getClientNickOrDefault(client_fd);
        if (input.params.size() < 2)
        {
              std::string err  = ":ft_irc 461 " + nick
                         + " KICK :Syntax error (KICK <#channel> <nick>)\r\n";
              send(client_fd, err.c_str(), err.size(), 0);
            return;
        }
        std::string channel  = input.params[0];
        std::string nickname = input.params[1];

        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        
        int target_fd = -1;
        std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
        std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
        while (user_it != user_ite) {
            if (user_it->second == nickname) {
                target_fd = user_it->first;
                break;
            }
            user_it++;
        }
        if (target_fd == -1)
        {
            std::string err  = ":ft_irc 441 " + nick + " "
                            + nickname + " " + channel
                            + " :They aren't on that channel\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            return ;
        }

        std::string prefix  = makePrefix(client_fd);
        std::string kickMsg = prefix + " KICK " + channel + " "
                            + nickname + "\r\n";
        std::map<int, std::string>::const_iterator uit  = it->second.getUsers().begin();
        std::map<int, std::string>::const_iterator uite = it->second.getUsers().end();
        while (uit != uite)
        {
            send(uit->first, kickMsg.c_str(), kickMsg.size(), 0);
            uit++;
        }
        
        it->second.removeUser(target_fd);
        it->second.removeOperator(target_fd);
        
        if (it->second.getUsers().empty())
        {
            std::cout << "Channel " << channel
                    << " deleted (no more users)" << std::endl;
            _channels.erase(it);
        }

        std::cout << "Client fd " << target_fd
                << " has been kicked from the channel: "
                << channel << std::endl;
    }
    else if (input.cmd == "INVITE") {
        std::string nick = getClientNickOrDefault(client_fd);
        if (input.params.size() < 2)
        {
            std::string err  = ":ft_irc 461 " + nick
                             + " INVITE :Syntax error (INVITE <#channel> <nick>)\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            return;
        }
        std::string channel  = input.params[0];
        std::string nickname = input.params[1];

        std::map<std::string, Channel>::iterator it_channel = _channels.find(channel);

        int target_fd = -1;
        std::map<int, std::string>::iterator it = _clientNicknames.begin();
        std::map<int, std::string>::iterator ite = _clientNicknames.end();
        while (it != ite) {
            if (it->second == nickname) {
                target_fd = it->first;
                break;
            }
            it++;
        }
        if (target_fd == -1)
        {
            std::string err = ":ft_irc 401 " + nick + " " + nickname + " :No such nick/channel\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The client was not found for the nickname: " << nickname << std::endl;
            return;
        }
        const std::map<int, std::string> &users = it_channel->second.getUsers();
        if (users.find(target_fd) != users.end())
        {
            std::string err = ":ft_irc 443 " + nick + " " + nickname + " " + channel
                            + " :is already on channel\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "Client " << nickname << " is already on channel "
                    << channel << std::endl;
            return;
        }

        it_channel->second.setInvitedUser(target_fd, nickname);
        std::string inviter = getClientNickOrDefault(client_fd);

        std::string confirm = ":ft_irc 341 " + inviter + " " + nickname + " " + channel + "\r\n";
        send(client_fd, confirm.c_str(), confirm.size(), 0);

        std::string inviteMsg = makePrefix(client_fd) + " INVITE " + nickname + " :" + channel + "\r\n";
        send(target_fd, inviteMsg.c_str(), inviteMsg.size(), 0);
    }
}