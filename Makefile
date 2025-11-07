CC=g++

CPPFLAGS+=-I/usr/include/postgresql -L/usr/lib/x86_64-linux-gnu -lpq -lpthread

all: server read_client write_client

server: src/server.cpp
	$(CC) -o build/server src/server.cpp src/DB.cpp $(CPPFLAGS) 

read_client: src/read_client.cpp
	$(CC) -o build/read_client src/read_client.cpp -lpthread

write_client: src/write_client.cpp
	$(CC) -o build/write_client src/write_client.cpp -lpthread
