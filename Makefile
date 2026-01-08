CC = g++
CFLAGS = -std=c++11 -Wall -Wextra

SERVER = pokemon_server
CLIENT = pokemon_client

all: $(SERVER) $(CLIENT)

# Compile server: First compile sqlite3.c as C, then link with C++ code
$(SERVER): pokemon_server.cpp sqlite3.o
	$(CC) $(CFLAGS) -o $(SERVER) pokemon_server.cpp sqlite3.o -lpthread -ldl

# Compile sqlite3.c as C code (not C++)
sqlite3.o: sqlite3.c sqlite3.h
	gcc -c sqlite3.c -o sqlite3.o

# Compile client
$(CLIENT): pokemon_client.cpp
	$(CC) $(CFLAGS) -o $(CLIENT) pokemon_client.cpp

clean:
	rm -f $(SERVER) $(CLIENT) *.o pokemon_store.db

.PHONY: all clean