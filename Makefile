NAME        := ircserv
BONUS_NAME  := ircserv

CXX         := c++
CXXFLAGS    := -Wall -Wextra -Werror -std=c++98 -Iinclude
DEPFLAGS    := -MMD -MP

SRC_DIR     := src
OBJ_DIR     := obj
OBJ_DIR_B   := obj_bonus

SRCS        := main.cpp \
               Server.cpp \
               Client.cpp \
               Channel.cpp \
               Message.cpp \
               CommandHandler.cpp \
               cmd_auth.cpp \
               cmd_chat.cpp \
               cmd_channel.cpp \
               cmd_operator.cpp \
               Utils.cpp

BONUS_SRCS  := $(SRCS) Bot.cpp

OBJS        := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS        := $(OBJS:.o=.d)

OBJS_B      := $(BONUS_SRCS:%.cpp=$(OBJ_DIR_B)/%.o)
DEPS_B      := $(OBJS_B:.o=.d)

RM          := rm -f

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

bonus: $(BONUS_NAME).bonus
	@cp $(BONUS_NAME).bonus $(NAME)

$(BONUS_NAME).bonus: $(OBJS_B)
	$(CXX) $(CXXFLAGS) -DBONUS -o $@ $(OBJS_B)

$(OBJ_DIR_B)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR_B)
	$(CXX) $(CXXFLAGS) -DBONUS $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR_B):
	@mkdir -p $(OBJ_DIR_B)

clean:
	$(RM) -r $(OBJ_DIR) $(OBJ_DIR_B)

fclean: clean
	$(RM) $(NAME) $(BONUS_NAME).bonus

re: fclean all

-include $(DEPS) $(DEPS_B)

.PHONY: all bonus clean fclean re
