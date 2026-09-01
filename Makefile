CC = gcc
CFLAGS = -std=c17 -D_GNU_SOURCE -Wall -Wextra
LDFLAGS = -lenet -lmsgpackc

SRC_DIR = src

all: server client

server: $(SRC_DIR)/server.c
	$(CC) $(CFLAGS) $< -o server.out $(LDFLAGS)

client: $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) $< -o client.out $(LDFLAGS)

sanclient: $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address -fno-omit-frame-pointer $< -o client.out $(LDFLAGS)

clean:
	rm -f server.out client.out
