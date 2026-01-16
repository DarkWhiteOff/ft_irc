NAME = ircserv

SRC = \
    srcs/server/Server.cpp \
    srcs/server/Channel.cpp \
    srcs/server/HandleClient.cpp \
    srcs/irc/HandleCommand.cpp \
    srcs/irc/HandleJoinPart.cpp \
    srcs/irc/HandleOpsCommand.cpp \
    srcs/irc/HandleModeCommand.cpp \
    srcs/irc/IrcReplies.cpp.cpp \
    srcs/utils/Exception.cpp \
    srcs/main.cpp
HEADERS = includes/Server.hpp includes/Channel.hpp

OBJS = $(SRC:%.cpp=%.o)

CC = c++
FLAGS = -Wall -Wextra -Werror -std=c++98 -g -I ./includes

%.o: %.cpp $(HEADERS)
	$(CC) $(FLAGS) -c $< -o $@

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) -o $(NAME)

clean:
	rm -rf $(OBJS)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re