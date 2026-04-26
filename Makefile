CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

SRC_DIR = src
BUILD_DIR = build

SERVER_TARGET = bomberman_server
CLIENT_TARGET = client

SERVER_SRC = src/server.c src/clients.c src/network.c src/game.c
CLIENT_SRC = LU_Bomberman.c src/network.c src/game.c

SERVER_OBJ = $(BUILD_DIR)/server.o $(BUILD_DIR)/clients.o $(BUILD_DIR)/network.o $(BUILD_DIR)/game_server.o
CLIENT_OBJ = $(BUILD_DIR)/LU_Bomberman.o $(BUILD_DIR)/network.o $(BUILD_DIR)/game_client.o

all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Server build: game.c compiled with -DSERVER_BUILD
$(BUILD_DIR)/game_server.o: src/game.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSERVER_BUILD -c $< -o $@

# Client build: game.c compiled WITHOUT -DSERVER_BUILD
$(BUILD_DIR)/game_client.o: src/game.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_TARGET): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ -lncurses

$(BUILD_DIR)/LU_Bomberman.o: LU_Bomberman.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean