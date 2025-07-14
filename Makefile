RED     = \033[31m
GREEN   = \033[32m
PURPLE  = \033[35m
RESET   = \033[0m

CC      = gcc
LD      = ld
CFLAGS  = -Wall -Wextra -Werror
LDFLAGS = -r -b binary
OPENSSL_LIBS = -lcrypto

B_DIR   = ./build

NAME = ft_shield
PAYLOAD = payload

SRC             = ft_shield.c
OBJS            = $(addprefix $(B_DIR)/, $(SRC:.c=.o))

PAYLOAD_SRC = payload.c
HEADER_SRC = ft_shield.h
PAYLOAD_OBJ = $(B_DIR)/payload.o

.PHONY: all clean fclean re install

all: $(NAME)

$(B_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@$(CC) -I$(HEADER_SRC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)[+] Compiled $<$(RESET)"

$(NAME): $(OBJS) $(PAYLOAD_OBJ) $(HEADER_SRC)
	@echo "$(GREEN)[+] Linking $(NAME)$(RESET)"
	@$(CC) $(CFLAGS) $(OBJS) $(PAYLOAD_OBJ) -o $(NAME)
	@echo "$(PURPLE)[+] $(NAME) ready$(RESET)"

$(PAYLOAD): $(PAYLOAD_SRC)
	@echo "$(GREEN)[+] Building payload binary$(RESET)"
	@$(CC) $(CFLAGS) $(PAYLOAD_SRC) -o $(PAYLOAD) $(OPENSSL_LIBS)

$(PAYLOAD_OBJ): $(PAYLOAD)
	@echo "$(GREEN)[+] Embedding payload$(RESET)"
	@$(LD) $(LDFLAGS) $(PAYLOAD) -o $(PAYLOAD_OBJ)

install: $(NAME)
	@echo "$(GREEN)[+] Installing ft_shield (requires root)$(RESET)"
	@./$(NAME)

clean:
	@echo "$(RED)[-] Cleaning build files$(RESET)"
	@rm -rf $(B_DIR) $(PAYLOAD)

fclean: clean
	@echo "$(RED)[-] Removing $(NAME)$(RESET)"
	@rm -f $(NAME)

re: fclean all