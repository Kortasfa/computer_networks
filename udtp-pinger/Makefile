CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17

CLIENT_TARGET = udp_pinger_client
SERVER_TARGET = udp_pinger_server

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): udp_pinger_client.cpp
	$(CXX) $(CXXFLAGS) -o $(CLIENT_TARGET) udp_pinger_client.cpp

$(SERVER_TARGET): udp_pinger_server.cpp
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) udp_pinger_server.cpp

clean:
	rm -f $(CLIENT_TARGET) $(SERVER_TARGET)

.PHONY: all clean


