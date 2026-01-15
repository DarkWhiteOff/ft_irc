#include "Server.hpp" //
#include <sstream> //

// Retourne le nick du client ou user<fd> //
std::string Server::getClientNickOrDefault(int client_fd) //
{ //
    std::map<int, std::string>::iterator it = _clientNicknames.find(client_fd); //
    if (it != _clientNicknames.end() && !it->second.empty()) //
        return it->second; //
    std::ostringstream oss; //
    oss << "user" << client_fd; //
    return oss.str(); //
} //

// Préfixe IRC :nick!server //
std::string Server::makePrefix(int client_fd) //
{ //
    std::string nick = getClientNickOrDefault(client_fd); //
    return ":" + nick + "!" + SERVER_NAME; //
} //

// Met à jour le nickname dans tous les channels où le client est présent //
void Server::updateNickInChannels(int client_fd, const std::string &oldNick, const std::string &newNick) //
{ //
    (void)oldNick; // on ne s’en sert pas vraiment, mais on le garde pour debug si besoin //
    std::map<std::string, Channel>::iterator it = _channels.begin(); //
    std::map<std::string, Channel>::iterator ite = _channels.end(); //
    while (it != ite) //
    { //
        const std::map<int, std::string> &users = it->second.getUsers(); //
        if (users.find(client_fd) != users.end()) //
        { //
            it->second.setUser(client_fd, newNick); //
        } //
        const std::map<int, std::string> &ops = it->second.getOperators(); //
        if (ops.find(client_fd) != ops.end()) //
        { //
            it->second.setOperator(client_fd, newNick); //
        } //
        ++it; //
    } //
} //

// Envoie :oldNick!server NICK :newNick à tous les channels où est le client //
void Server::broadcastNickChange(int client_fd, const std::string &oldNick, const std::string &newNick) //
{ //
    if (oldNick.empty() || oldNick == newNick) //
        return; //

    std::string prefix = ":" + oldNick + "!" + SERVER_NAME; //
    std::string line = prefix + " NICK :" + newNick + "\r\n"; //

    // Toujours envoyer au client lui-même pour qu'irssi mette à jour son nick local //
    send(client_fd, line.c_str(), line.size(), 0); //

    // Ensuite, envoyer à tous les autres clients qui partagent un channel avec lui //
    std::map<std::string, Channel>::iterator it = _channels.begin(); //
    std::map<std::string, Channel>::iterator ite = _channels.end(); //
    while (it != ite) //
    { //
        const std::map<int, std::string> &users = it->second.getUsers(); //
        if (users.find(client_fd) != users.end()) //
        { //
            std::map<int, std::string>::const_iterator uit = users.begin(); //
            std::map<int, std::string>::const_iterator uite = users.end(); //
            while (uit != uite) //
            { //
                if (uit->first != client_fd) //
                    send(uit->first, line.c_str(), line.size(), 0); //
                ++uit; //
            } //
        } //
        ++it; //
    } //
} //

// 353 + 366 pour /NAMES //
void Server::sendNamesReply(int client_fd, const std::string &channel, Channel &chan) //
{ //
    std::string nick = getClientNickOrDefault(client_fd); //
    const std::map<int, std::string> &users = chan.getUsers(); //
    const std::map<int, std::string> &ops = chan.getOperators(); //

    std::string names = ":" SERVER_NAME " 353 " + nick + " = " + channel + " :"; //
    std::map<int, std::string>::const_iterator it = users.begin(); //
    std::map<int, std::string>::const_iterator ite = users.end(); //
    while (it != ite) //
    { //
        if (ops.find(it->first) != ops.end()) //
            names += "@" + it->second + " "; //
        else //
            names += it->second + " "; //
        ++it; //
    } //
    names += "\r\n"; //
    send(client_fd, names.c_str(), names.length(), 0); //

    std::string endNames = ":" SERVER_NAME " 366 " + nick + " " + channel + " :End of /NAMES list.\r\n"; //
    send(client_fd, endNames.c_str(), endNames.length(), 0); //
} //

// 331/332 pour le topic //
void Server::sendTopicReply(int client_fd, const std::string &channel, Channel &chan) //
{ //
    std::string nick = getClientNickOrDefault(client_fd); //
    const std::string &topic = chan.getTopic(); //

    std::string reply; //
    if (topic.empty()) //
        reply = ":" SERVER_NAME " 331 " + nick + " " + channel + " :No topic is set\r\n"; //
    else //
        reply = ":" SERVER_NAME " 332 " + nick + " " + channel + " :" + topic + "\r\n"; //

    send(client_fd, reply.c_str(), reply.size(), 0); //
} //

// JOIN propre //
void Server::IrssiJoin(int client_fd, const std::string& channel) //
{ //
    std::map<std::string, Channel>::iterator it = _channels.find(channel); //
    if (it == _channels.end()) //
        return; //

    std::string prefix = makePrefix(client_fd); //
    std::string joinLine = prefix + " JOIN :" + channel + "\r\n"; //

    const std::map<int, std::string> &users = it->second.getUsers(); //
    std::map<int, std::string>::const_iterator user_it = users.begin(); //
    std::map<int, std::string>::const_iterator user_ite = users.end(); //
    while (user_it != user_ite) //
    { //
        send(user_it->first, joinLine.c_str(), joinLine.length(), 0); //
        ++user_it; //
    } //

    sendTopicReply(client_fd, channel, it->second); //
    sendNamesReply(client_fd, channel, it->second); //
} //

// PRIVMSG channel //
void Server::sendChannelPrivmsg(int client_fd, const std::string &target, const std::string &text) //
{ //
    std::map<std::string, Channel>::iterator it = _channels.find(target); //
    if (it == _channels.end()) //
        return; //

    const std::map<int, std::string> &users = it->second.getUsers(); //
    if (users.find(client_fd) == users.end()) //
    { //
        std::string nick = getClientNickOrDefault(client_fd); //
        std::string err = ":" SERVER_NAME " 404 " + nick + " " + target + " :Cannot send to channel\r\n"; //
        send(client_fd, err.c_str(), err.size(), 0); //
        return; //
    } //

    std::string prefix = makePrefix(client_fd); //
    std::string line = prefix + " PRIVMSG " + target + " :" + text + "\r\n"; //

    std::map<int, std::string>::const_iterator itu = users.begin(); //
    std::map<int, std::string>::const_iterator ite = users.end(); //
    while (itu != ite) //
    { //
        if (itu->first != client_fd) //
            send(itu->first, line.c_str(), line.size(), 0); //
        ++itu; //
    } //
} //

// PRIVMSG privé //
void Server::sendPrivatePrivmsg(int client_fd, const std::string &target, const std::string &text) //
{ //
    int target_fd = -1; //
    std::map<int, std::string>::iterator it = _clientNicknames.begin(); //
    std::map<int, std::string>::iterator ite = _clientNicknames.end(); //
    while (it != ite) //
    { //
        if (it->second == target) //
        { //
            target_fd = it->first; //
            break; //
        } //
        ++it; //
    } //

    std::string nick = getClientNickOrDefault(client_fd); //
    if (target_fd == -1) //
    { //
        std::string err = ":" SERVER_NAME " 401 " + nick + " " + target + " :No such nick/channel\r\n"; //
        send(client_fd, err.c_str(), err.size(), 0); //
        return; //
    } //

    std::string prefix = makePrefix(client_fd); //
    std::string line = prefix + " PRIVMSG " + target + " :" + text + "\r\n"; //
    send(target_fd, line.c_str(), line.size(), 0); //
    std::cout << "Sending private message from " << nick << " to " << target << ": " << text << std::endl; //
} //
