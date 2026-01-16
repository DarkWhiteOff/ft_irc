#include "Server.hpp"

void Server::handleModeCommand(int client_fd, const std::string& message)
{
    (void) client_fd;
    std::string params = message.substr(5);
    std::string channel;
    std::string mode;
    std::string mode_args;
    if (params.find(' ') == std::string::npos)
        return;
    channel = params.substr(0, params.find(' '));
    std::string rest = params.substr(params.find(' ') + 1);
    if (rest.find(' ') == std::string::npos) {
        mode = rest;
    } else {
        mode      = rest.substr(0, rest.find(' '));
        mode_args = rest.substr(rest.find(' ') + 1);
    }

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it == _channels.end()) {
        std::cout << "Channel was not found for the name: "
                  << channel << std::endl;
        return;
    }
    for (size_t i = 0; i < mode.length(); ++i) {
        if (mode[i] == '+') {
            ++i;
            while (i < mode.length()) {
                switch (mode[i]) {
                    case 'i':
                        it->second.setI(true);
                        break;
                    case 't':
                        it->second.setT(true);
                        break;
                    case 'k':
                        it->second.setK(true, mode_args);
                        break;
                    case 'o': {
                        std::map<int, std::string>::const_iterator it_users = it->second.getUsers().begin();
                        std::map<int, std::string>::const_iterator ite_users = it->second.getUsers().end();
                        bool user_found = false;
                        while (it_users != ite_users) {
                            if (it_users->second == mode_args) {
                                it->second.setOperator(it_users->first, mode_args);
                                user_found = true;
                                break;
                            }
                            ++it_users;
                        }
                        if (!user_found) {
                            std::cout << "The user " << mode_args
                                        << " is not in the channel " << channel
                                        << std::endl;
                        }
                        break;
                    }
                    case 'l':
                        it->second.setL(true, std::atoi(mode_args.c_str()));
                        break;
                    default:
                        break;
                }
                ++i;
            }
            --i;
        } else if (mode[i] == '-') {
            ++i;
            while (i < mode.length()) {
                switch (mode[i]) {
                    case 'i':
                        it->second.setI(false);
                        break;
                    case 't':
                        it->second.setT(false);
                        break;
                    case 'k':
                        it->second.setK(false, "");
                        break;
                    case 'o': {
                        std::map<int, std::string>::const_iterator it_ops = it->second.getOperators().begin();
                        std::map<int, std::string>::const_iterator ite_ops = it->second.getOperators().end();
                        bool op_found = false;
                        while (it_ops != ite_ops) {
                            if (it_ops->second == mode_args) {
                                it->second.removeOperator(it_ops->first);
                                op_found = true;
                                break;
                            }
                            ++it_ops;
                        }
                        if (!op_found) {
                            std::cout << "The operator " << mode_args
                                        << " is not in the channel " << channel
                                        << std::endl;
                        }
                        break;
                    }
                    case 'l':
                        it->second.setL(false, 0);
                        break;
                    default:
                        break;
                }
                ++i;
            }
            --i;
        }
    }
}