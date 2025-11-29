CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread

SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

SERVER_SRCS = $(SRC_DIR)/main.cpp $(SRC_DIR)/server.cpp
TEST_SRCS = $(TEST_DIR)/test_server.cpp $(SRC_DIR)/server.cpp

SERVER_TARGET = webserver
TEST_TARGET = run_tests

all: $(SERVER_TARGET)

$(SERVER_TARGET): $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_SRCS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(TEST_SRCS)

clean:
	rm -f $(SERVER_TARGET) $(TEST_TARGET)

.PHONY: all test clean
