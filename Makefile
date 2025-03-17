NAME = chat-from-term

SRC = chat_server.c

$(NAME):
	cc -Wall -Wextra -Werror $(SRC) -o $(NAME)

fclean:
	rm $(NAME)

.PHONY: fclean
