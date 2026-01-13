NAME = ircserv

SRC = srcs/Server.cpp srcs/Channel.cpp srcs/client/HandleClient.cpp \
		srcs/client/HandleCommand.cpp srcs/client/HandleOpsCommand.cpp \
		srcs/client/HandleModeCommand.cpp \
		srcs/utils/exception.cpp srcs/main.cpp
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