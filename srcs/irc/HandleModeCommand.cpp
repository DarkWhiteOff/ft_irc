#include "Server.hpp"

void Server::handleModeCommand(int client_fd, const UserInput &input)
{
    std::string nick = getClientNickOrDefault(client_fd);
    if (input.params.size() < 2)
    {
        std::string err  = ":ft_irc 461 " + nick
                           + " MODE :Not enough parameters\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }
    std::string channel     = input.params[0];
    std::string modeString  = input.params[1];

    std::map<std::string, Channel>::iterator it = _channels.find(channel);
    std::vector<std::string> modeParams;
    for (size_t i = 2; i < input.params.size(); i++)
        modeParams.push_back(input.params[i]);

    bool  status = true;
    char  sign   = '+';
    size_t start = 0;

    if (!modeString.empty() && (modeString[0] == '+' || modeString[0] == '-'))
    {
        sign   = modeString[0];
        status = (sign == '+');
        start  = 1;
    }
    else
    {
        sign   = '+';
        status = true;
        start  = 0;
    }

    std::string              appliedModes;
    std::vector<std::string> appliedParams;
    size_t                   paramIndex = 0;
    for (size_t i = start; i < modeString.size(); i++) {
        char c = modeString[i];

        switch (c)
        {
            case 'i':
            {
                it->second.setI(status);
                if (appliedModes.empty())
                    appliedModes += sign;
                appliedModes += 'i';
                break;
            }
            case 't':
            {
                it->second.setT(status);
                if (appliedModes.empty())
                    appliedModes += sign;
                appliedModes += 't';
                break;
            }
            case 'k':
            {
                if (status)
                {
                    if (paramIndex >= modeParams.size())
                    {
                        std::string err = ":ft_irc 461 " + nick
                                        + " MODE :Syntax error (MODE <#channel> +k <key>)\r\n";
                        send(client_fd, err.c_str(), err.size(), 0);
                        return ;
                    }
                    std::string key = modeParams[paramIndex++];
                    it->second.setK(true, key);

                    if (appliedModes.empty())
                        appliedModes += sign;
                    appliedModes += 'k';
                    appliedParams.push_back(key);
                }
                else
                {
                    it->second.setK(false, "");
                     if (appliedModes.empty())
                        appliedModes += sign;
                    appliedModes += 'k';
                }
                break;
            }
            case 'o':
            {
                if (paramIndex >= modeParams.size())
                {
                    std::string err = ":ft_irc 461 " + nick
                                    + " MODE :Syntax error (MODE <#channel> "
                                      "+o/-o <nick>)\r\n";
                    send(client_fd, err.c_str(), err.size(), 0);
                    return;
                }

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
                        uit++;
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
                        oit++;
                    }
                    if (!op_found)
                    {
                        std::cout << "The operator " << nick
                                  << " is not in the channel " << channel
                                  << std::endl;
                    }
                }
                if (appliedModes.empty())
                    appliedModes += sign;
                appliedModes += 'o';
                appliedParams.push_back(nick);
                break;
            }
            case 'l':
            {
                if (status)
                {
                    if (paramIndex >= modeParams.size())
                    {
                        std::string err = ":ft_irc 461 " + nick
                                        + " MODE :Syntax error (MODE <#channel> +l <limit>)\r\n";
                        send(client_fd, err.c_str(), err.size(), 0);
                        return;

                    }
                    std::string limitStr = modeParams[paramIndex++];
                    int limit = std::atoi(limitStr.c_str());
                    if (limit <= 0)
                        break;
                    
                    it->second.setL(true, limit);

                    if (appliedModes.empty())
                        appliedModes += sign;
                    appliedModes += 'l';
                    appliedParams.push_back(limitStr);
                }
                else
                {
                    it->second.setL(false, 0);
                    if (appliedModes.empty())
                        appliedModes += sign;
                    appliedModes += 'l';
                }
                break;
            }
            default:
            {
                std::string unknown(1, c);
                std::string err = ":ft_irc 472 " + nick + " " + unknown
                                + " :is unknown mode char\r\n";
                send(client_fd, err.c_str(), err.size(), 0);
                break;
            }
        }
    }
    if (appliedModes.empty())
        return;

    const std::map<int, std::string> &users = it->second.getUsers();

    std::string prefix = makePrefix(client_fd);
    std::string line   = prefix + " MODE " + channel + " " + appliedModes;

    for (size_t i = 0; i < appliedParams.size(); i++)
    {
        line += " ";
        line += appliedParams[i];
    }
    line += "\r\n";

    std::map<int, std::string>::const_iterator uit  = users.begin();
    std::map<int, std::string>::const_iterator uite = users.end();
    while (uit != uite)
    {
        send(uit->first, line.c_str(), line.size(), 0);
        uit++;
    }
}
