CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread -I./include -I./common

SERVER_SRCS = server/ServerMain.cpp
CLIENT_SRCS = client/ClientMain.cpp

all: server_app client_app

server_app:
	$(CXX) $(CXXFLAGS) $(SERVER_SRCS) -o server_app -lboost_system

client_app:
	$(CXX) $(CXXFLAGS) $(CLIENT_SRCS) -o client_app -lboost_system

clean:
	rm -f server_app client_app
