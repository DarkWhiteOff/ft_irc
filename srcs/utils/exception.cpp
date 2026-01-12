#include "Server.hpp"

const char *Server::SocketCreationException::what(void) const throw()
{
    return "Error : Failed to create socket";
}

const char *Server::setsockoptException::what(void) const throw()
{
    return "Error : Failed to setsockopt(SO_REUSEADDR)";
}

const char *Server::SocketBindException::what(void) const throw()
{
    return "Error : Failed to bind (port may already be in use)";
}

const char *Server::SocketListenException::what(void) const throw()
{
    return "Error : Failed to listen";
}