CC = gcc
CFLAGS = -Wall -Wextra

all: server client1 client2 client3
	@echo "Make worked"

server: server.cpp
	$(CC) $(CFLAGS) server.cpp -o server

client1: client1.cpp
	$(CC) $(CFLAGS) client1.cpp -o client1

client2: client2.cpp
	$(CC) $(CFLAGS) client2.cpp -o client2

client3: client3.cpp
	$(CC) $(CFLAGS) client3.cpp -o client3

clean:
	@echo "removing everything but source"
	rm -f server client1 client2 client3	
