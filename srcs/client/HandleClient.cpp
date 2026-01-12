#include "Server.hpp"

void Server::removeClient(int client_fd, fd_set &masterSet)
{
    std::cout << "Client disconnected, socket fd: " << client_fd << std::endl;

    std::map<std::string, Channel>::iterator ch_it  = _channels.begin();
    std::map<std::string, Channel>::iterator ch_ite = _channels.end();
    while (ch_it != ch_ite) {
        ch_it->second.removeUser(client_fd);
        ch_it->second.removeOperator(client_fd);
        ++ch_it;
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

    _clientRegistered[client_fd] = true;

    // Envoi des replies minimales attendues par un client IRC (001-004) //
    sendReply(client_fd, "001", nick, "Welcome to the ft_irc server!");
    sendReply(client_fd, "002", nick, "Your host is " SERVER_NAME ", running version 1.0");
    sendReply(client_fd, "003", nick, "This server was created just now");
    sendReply(client_fd, "004", nick, SERVER_NAME " 1.0 oi oi");
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
            if (message.rfind("PASS ", 0) == 0) {
                std::string password = message.substr(5);
                if (password == _password) {
                    _clientAuthentifieds[client_fd] = true;
                    std::cout << "Client fd " << client_fd << " authentified successfully." << std::endl;
                } else {
                    std::cout << "Client fd " << client_fd << " failed authentication." << std::endl;
                    send(client_fd, "Invalid password.\r\n", 19, 0);
                }
            }
            else if (_clientAuthentifieds[client_fd])
            {
                handleClientMessage(client_fd, message);
                tryRegisterClient(client_fd);
            }
            else {
                send(client_fd,
                 "You must authenticate first using PASS command.\r\n", 52, 0);
                std::cout << "Client fd " << client_fd
                      << " is not authenticated. Ignored message: "
                      << message << std::endl;
            }
        }
    }
}