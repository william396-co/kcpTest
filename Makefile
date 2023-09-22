src=$(wildcard src/*.cpp src/*.c)

all: server client multi_client test

clean:
	rm -rf kcp_server kcp_client kcp_multi_client kcp_test

server:
	g++ -std=c++17 -lpthread -Wall -g $(src) server.cpp kcp_server.cpp -o kcp_server

client:
	g++ -std=c++17 -lpthread -Wall -g $(src) client.cpp kcp_client.cpp -o kcp_client

multi_client:
	g++ -std=c++17 -lpthread -Wall -g $(src) client.cpp kcp_client.cpp -o kcp_multi_client

test:
	g++ -std=c++17 -lpthread -g $(src) test.cpp -o kcp_test


