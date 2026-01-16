#include "Server.hpp"

void Server::removeClient(int client_fd, fd_set &masterSet)
{
    std::cout << "Client disconnected, socket fd: " << client_fd << std::endl;
    std::string quitLine = makePrefix(client_fd) + " QUIT :Client disconnected\r\n";

    std::map<std::string, Channel>::iterator ch_it  = _channels.begin();
    while (ch_it != _channels.end()) {
        std::map<int, std::string>::const_iterator uit = ch_it->second.getUsers().begin();
        while (uit != ch_it->second.getUsers().end())
        {
            if (uit->first != client_fd)
                send(uit->first, quitLine.c_str(), quitLine.size(), 0);
            uit++;
        }

        ch_it->second.removeUser(client_fd);
        ch_it->second.removeOperator(client_fd);
        if (ch_it->second.getUsers().empty()) {
            std::cout << "Channel " << ch_it->first
                      << " deleted (no more users)" << std::endl;
            _channels.erase(ch_it++);
        }
        else
            ch_it++;
    }

    _clientBuffers.erase(client_fd);
    _clientAuthentifieds.erase(client_fd);
    _clientNicknames.erase(client_fd);
    _clientUsernames.erase(client_fd);
    _clientRegistered.erase(client_fd);

    close(client_fd);
    FD_CLR(client_fd, &masterSet);
}

void Server::tryRegisterClient(int client_fd)
{
    if (_clientRegistered[client_fd])
        return;

    std::map<int, bool>::iterator itAuth = _clientAuthentifieds.find(client_fd);
    bool hasPass = (itAuth != _clientAuthentifieds.end() && itAuth->second);
    if (!hasPass)
        return;

    std::map<int, std::string>::iterator itNick = _clientNicknames.find(client_fd);
    if (itNick == _clientNicknames.end())
        return;

    std::map<int, std::string>::iterator itUser = _clientUsernames.find(client_fd);
    if (itUser == _clientUsernames.end())
        return;

    const std::string &nick = itNick->second;
    const std::string &user = itUser->second;

    _clientRegistered[client_fd] = true;

    std::string numeric001 = ":ft_irc 001 " + nick + " :Welcome to the ft_irc Network, " + nick + "!" + user + "@ft_irc\r\n";
    send(client_fd, numeric001.c_str(), numeric001.size(), 0);
    
    std::string numeric002 = ":ft_irc 002 " + nick + " :Your host is <servername>, running version 1.0\r\n";
    send(client_fd, numeric002.c_str(), numeric002.size(), 0);
    
    std::string numeric003 = ":ft_irc 003 " + nick + " :This server was created on just now\r\n";
    send(client_fd, numeric003.c_str(), numeric003.size(), 0);
    
    std::string numeric004 = ":ft_irc 004 " + nick + " ft_irc 1.0 - itkol\r\n";
    send(client_fd, numeric004.c_str(), numeric004.size(), 0);
    std::cout << "Client " << nick << " (" << user << ") has completed registration" << std::endl;
}

void Server::handleClientData(int client_fd, fd_set &masterSet, int maxFd)
{
    (void)maxFd;
    char buffer[4096];
    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        if (bytes == 0) {
            removeClient(client_fd, masterSet);
        } else {
            std::cerr << "Error : Failed to receive data on fd "
                      << client_fd << std::endl;
            removeClient(client_fd, masterSet);
        }
        return ;
    } else {
        _clientBuffers[client_fd].append(buffer, bytes);
        size_t eol_pos;
        while ((eol_pos = _clientBuffers[client_fd].find("\r\n")) != std::string::npos) {
            std::string message = _clientBuffers[client_fd].substr(0, eol_pos);
            _clientBuffers[client_fd].erase(0, eol_pos + 2);
            if (message.empty())
                continue;

            if (message.rfind("QUIT", 0) == 0) {
                removeClient(client_fd, masterSet);
                return;
            }
            if (message.rfind("PASS ", 0) == 0) {
                std::string password = message.substr(5);
                if (password == _password) {
                    _clientAuthentifieds[client_fd] = true;
                    std::cout << "Client fd " << client_fd << " authentified successfully." << std::endl;
                } else {
                    std::string err = ":ft_irc 464 * :Password incorrect\r\n";
                    send(client_fd, err.c_str(), err.size(), 0);
                    std::cout << "Client fd " << client_fd << " failed authentication." << std::endl;
                }
            }
            else if (message.rfind("CAP ", 0) == 0) {
                std::cout << "Client fd " << client_fd << " sent CAP command: " << message << std::endl;
            }
            else if (_clientAuthentifieds[client_fd])
            {
                handleClientMessage(client_fd, message);
                tryRegisterClient(client_fd);
            }
            else {
                std::cout << "Client fd " << client_fd
                      << " is not authenticated. Ignored message: "
                      << message << std::endl;
            }
        }
    }
}