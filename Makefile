CC=g++

CFLAGS+=-I/usr/include/postgresql -L/usr/lib/x86_64-linux-gnu -lpq -lpthread

server: src/server.cpp
	$(CC) -o build/server src/server.cpp src/DB.cpp $(CFLAGS) 

read_client: src/read_client.cpp
	$(CC) -o build/read_client src/read_client.cpp -lpthread

write_client: src/write_client.cpp
	$(CC) -o build/write_client src/write_client.cpp -lpthread
