NAME		= yadb

CC			= clang
CFLAGS		= -Wall -Werror -Wextra -pedantic -std=c23 `pkg-config --cflags libdwarf`
LDFLAGS		= `pkg-config --libs libdwarf`

SRC_DIR		= src
BUILD_DIR	= build

SRCS 		= $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/ds/*.c)
OBJS		= $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))


all: $(BUILD_DIR)/$(NAME)

debug: CFLAGS += -g -fsanitize=address
debug: all

$(BUILD_DIR)/$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/

.PHONY: all, debug, clean
