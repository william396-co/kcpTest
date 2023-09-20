src=$(wildcard src/*.cpp src/*.c)

all: server client test

clean:
	rm -rf kcp_server kcp_client kcp_test

server:
	g++ -std=c++17 -lpthread -Wall -g $(src) server.cpp kcp_server.cpp -o kcp_server

client:
	g++ -std=c++17 -lpthread -Wall -g $(src) client.cpp kcp_client.cpp -o kcp_client

test:
	g++ -std=c++17 -lpthread -Wall -g $(src) test.cpp -o kcp_test


