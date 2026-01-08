NAME = ircserv

CXX = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98 -g

SRC = Server.cpp Channel.cpp main.cpp \

OBJS = ${SRC:.cpp=.o}

all: ${NAME}

${NAME}: ${OBJS}
	${CXX} ${CPPFLAGS} ${OBJS} -o ${NAME}

clean:
	rm -f *.o

fclean: clean
	rm -f ${NAME}

re: fclean all

.PHONY: all re clean fclean