#include "Server.hpp"

void Server::handleTopicCommand(int client_fd, const std::string& message)
{
    if (message.rfind("TOPIC ", 0) != 0)
        return;
    size_t pos = message.find("TOPIC") + 6;
    std::string params = message.substr(pos);

    if (params.find("\r") != std::string::npos)
        params.erase(params.find("\r"));
    if (params.find("\n") != std::string::npos)
        params.erase(params.find("\n"));

    std::string channel;
    std::string topic;

    size_t colonPos = params.find(" :");
    if (colonPos != std::string::npos) {
        channel = params.substr(0, colonPos);
        topic   = params.substr(colonPos + 2);
    } else {
        channel = params;
    }

    while (!channel.empty() && channel[0] == ' ')
        channel.erase(0, 1);

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it == _channels.end()) {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err = ":" SERVER_NAME " 403 " + nick + " " + channel + " :No such channel\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        std::cout << "Channel was not found for the name: "
                    << channel << std::endl;
        return;
    }

    const std::map<int, std::string> &users = it->second.getUsers();
    if (users.find(client_fd) == users.end())
    {
        std::string nick = getClientNickOrDefault(client_fd);
        std::string err = ":" SERVER_NAME " 442 " + nick + " " + channel + " :You're not on that channel\r\n";
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
        ++uit;
    }

    std::cout << "The topic of the channel " << channel << " has been set to: " << topic << std::endl;
}

void Server::handleOperatorCommands(int client_fd, const std::string& message)
{
    if (message.rfind("KICK ", 0) == 0) {
        std::string params  = message.substr(5);
        if (params.find(' ') == std::string::npos)
            return;
        std::string channel  = params.substr(0, params.find(' '));
        std::string nickname = params.substr(params.find(' ') + 1);

        size_t spacePos = nickname.find(' ');
        if (spacePos != std::string::npos)
            nickname = nickname.substr(0, spacePos);

        std::map<std::string, Channel>::iterator it = _channels.find(channel);
        if (it != _channels.end()) {
            std::map<int, std::string>::const_iterator user_it = it->second.getUsers().begin();
            std::map<int, std::string>::const_iterator user_ite = it->second.getUsers().end();
            while (user_it != user_ite) {
                if (user_it->second == nickname) {
                    int target_fd = user_it->first;
                    std::string opNick = _clientNicknames[client_fd];
                    std::string kickMsg = ":" + opNick + "!ft_irc KICK "
                                          + channel + " " + nickname + " :Kicked\r\n";
                    std::map<int, std::string>::const_iterator b_it = it->second.getUsers().begin();
                    std::map<int, std::string>::const_iterator b_ite = it->second.getUsers().end();
                    while (b_it != b_ite) {
                        send(b_it->first, kickMsg.c_str(), kickMsg.size(), 0);
                        b_it++;
                    }
                    it->second.removeUser(target_fd);
                    it->second.removeOperator(target_fd);
                    std::cout << "Client fd " << target_fd << " has been kicked from the channel: " << channel << std::endl;
                    break;
                }
                user_it++;
            }
        } else {
            std::cout << "The channel was not found for the name: " << channel << std::endl;
        }
    }
    else if (message.rfind("INVITE ", 0) == 0) {
        std::string params  = message.substr(7);
        if (params.find(' ') == std::string::npos)
            return;
        std::string channel  = params.substr(0, params.find(' '));
        std::string nickname = params.substr(params.find(' ') + 1);

        std::map<std::string, Channel>::iterator it_channel = _channels.find(channel);
        if (it_channel == _channels.end())
        {
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":" SERVER_NAME " 403 " + nick + " " + channel + " :No such channel\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The channel was not found for the name: " << channel << std::endl;
            return;
        }

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
            std::string nick = getClientNickOrDefault(client_fd);
            std::string err = ":" SERVER_NAME " 401 " + nick + " " + nickname + " :No such nick/channel\r\n";
            send(client_fd, err.c_str(), err.size(), 0);
            std::cout << "The client was not found for the nickname: " << nickname << std::endl;
            return;
        }

        it_channel->second.setInvitedUser(target_fd, nickname);
        std::string inviter = getClientNickOrDefault(client_fd);

        std::string confirm = ":" SERVER_NAME " 341 " + inviter + " " + nickname + " " + channel + "\r\n";
        send(client_fd, confirm.c_str(), confirm.size(), 0);

        std::string inviteMsg = makePrefix(client_fd) + " INVITE " + nickname + " :" + channel + "\r\n";
        send(target_fd, inviteMsg.c_str(), inviteMsg.size(), 0);

        std::cout << "Client fd " << target_fd << " has been invited to the channel: " << channel << std::endl;
    }
    else if (message.rfind("MODE ", 0) == 0) {
        handleModeCommand(client_fd, message);
    }
}
