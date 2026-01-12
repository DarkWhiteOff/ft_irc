#include "Server.hpp"
#include <iostream>

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