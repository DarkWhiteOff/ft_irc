#include "Server.hpp"

void Server::IrssiJoin(int client_fd, const std::string& channel) 
{
    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    const std::string &nick = _clientNicknames[client_fd];
        std::string joinLine = ":" + nick + " JOIN " + channel + "\r\n";
        const std::map<int, std::string> &users = it->second.getUsers();
        std::map<int, std::string>::const_iterator user_it = users.begin();
        std::map<int, std::string>::const_iterator user_ite = users.end();
        while (user_it != user_ite) {
            send(user_it->first, joinLine.c_str(), joinLine.length(), 0);
            ++user_it;
        }
        std::string names = ":" SERVER_NAME " 353 " + nick + " = " + channel + " :";
        user_it = users.begin();
        while (user_it != user_ite) {
            names += user_it->second + " ";
            ++user_it;
        }
        names += "\r\n";
        send(client_fd, names.c_str(), names.length(), 0);
        std::string endNames = ":" SERVER_NAME " 366 " + nick + " " + channel + " :End of /NAMES list.\r\n";
        send(client_fd, endNames.c_str(), endNames.length(), 0);
}