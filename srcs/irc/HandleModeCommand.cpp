#include "Server.hpp"

void Server::handleModeCommand(int client_fd, const UserInput &input)
{
    (void) client_fd;
     if (input.params.size() < 2)
        return;
    std::string channel     = input.params[0];
    std::string modeString  = input.params[1];

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    if (it == _channels.end()) {
        std::cout << "Channel was not found for the name: "
                  << channel << std::endl;
        return;
    }
    std::vector<std::string> modeParams;
    for (size_t i = 2; i < input.params.size(); ++i)
        modeParams.push_back(input.params[i]);

    size_t paramIndex = 0;
    bool status = true;

    for (size_t i = 0; i < modeString.size(); ++i) {
        char c = modeString[i];

        if (c == '+')
        {
            status = true;
            continue;
        }
        if (c == '-')
        {
            status = false;
            continue;
        }

        switch (c)
        {
            case 'i':
                it->second.setI(status);
                break;
            case 't':
                it->second.setT(status);
                break;
            case 'k':
            {
                if (status)
                {
                    if (paramIndex >= modeParams.size())
                        return;
                    std::string key = modeParams[paramIndex++];
                    it->second.setK(true, key);
                }
                else
                {
                    it->second.setK(false, "");
                }
                break;
            }
            case 'o':
            {
                if (paramIndex >= modeParams.size())
                    return;

                std::string nick = modeParams[paramIndex++];
                const std::map<int, std::string> &users = it->second.getUsers();
                const std::map<int, std::string> &ops   = it->second.getOperators();

                if (status)
                {
                    std::map<int, std::string>::const_iterator uit  = users.begin();
                    std::map<int, std::string>::const_iterator uite = users.end();
                    bool user_found = false;

                    while (uit != uite)
                    {
                        if (uit->second == nick)
                        {
                            it->second.setOperator(uit->first, nick);
                            user_found = true;
                            break;
                        }
                        ++uit;
                    }
                    if (!user_found)
                    {
                        std::cout << "The user " << nick
                                  << " is not in the channel " << channel
                                  << std::endl;
                    }
                }
                else
                {
                    std::map<int, std::string>::const_iterator oit  = ops.begin();
                    std::map<int, std::string>::const_iterator oite = ops.end();
                    bool op_found = false;

                    while (oit != oite)
                    {
                        if (oit->second == nick)
                        {
                            it->second.removeOperator(oit->first);
                            op_found = true;
                            break;
                        }
                        ++oit;
                    }
                    if (!op_found)
                    {
                        std::cout << "The operator " << nick
                                  << " is not in the channel " << channel
                                  << std::endl;
                    }
                }
                break;
            }
            case 'l':
            {
                if (status)
                {
                    if (paramIndex >= modeParams.size())
                        return;

                    int limit = std::atoi(modeParams[paramIndex++].c_str());
                    if (limit > 0)
                        it->second.setL(true, limit);
                }
                else
                {
                    it->second.setL(false, 0);
                }
                break;
            }
            default:
                break;
        }
    }
}
