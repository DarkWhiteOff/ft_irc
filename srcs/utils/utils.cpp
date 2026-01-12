#include "Server.hpp"

void Server::sendReply(int client_fd, const std::string &code,
                       const std::string &nickname, const std::string &message)
{
    std::string reply = ":" + std::string(SERVER_NAME) + " "
                        + code + " " + nickname + " :" + message + "\r\n";
    if (send(client_fd, reply.c_str(), reply.length(), 0) < 0) {
        std::cerr << "Error : send failed in sendReply" << std::endl;
    }
}