CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread

SRCS = main.cpp proxy_server.cpp
TARGET = proxy_server

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
	rm -rf cache

.PHONY: all clean

