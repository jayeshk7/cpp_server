CC=g++

CFLAGS+=-I/usr/include/postgresql -L/usr/lib/x86_64-linux-gnu -lpq -lpthread

server: src/server.cpp
	$(CC) -o src/server src/server.cpp src/DB.cpp $(CFLAGS) 

read_client: src/read_client.cpp
	$(CC) -o src/read_client src/read_client.cpp -lpthread

write_client: src/write_client.cpp
	$(CC) -o src/write_client src/write_client.cpp -lpthread
