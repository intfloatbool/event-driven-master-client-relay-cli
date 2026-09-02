CC = gcc
CFLAGS = -std=c17 -D_GNU_SOURCE -Wall -Wextra
LDFLAGS = -lenet -lmsgpackc

#main.c my_funcz.c
CLIENT_SRCS = src/client.c src/protocol_msg.c
SERVER_SRCS = src/server.c src/protocol_msg.c

all: server client

server: $(SERVER_SRCS)
	$(CC) $(CFLAGS) $^ -o server.out $(LDFLAGS)

client: $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $^ -o client.out $(LDFLAGS)

clean:
	rm -f server.out client.out
