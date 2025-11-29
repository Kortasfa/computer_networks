CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f server client

test: all
	python3 -m socket_programming_tests.test_valid
	python3 -m socket_programming_tests.test_shutdown
	python3 -m socket_programming_tests.test_concurrent

.PHONY: all clean test

CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f server client

.PHONY: all clean


