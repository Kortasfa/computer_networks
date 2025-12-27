CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SENDER_TARGET = rdt_sender
RECEIVER_TARGET = rdt_receiver

.PHONY: all clean

all: $(SENDER_TARGET) $(RECEIVER_TARGET)

$(SENDER_TARGET): rdt_sender.cpp rdt_common.hpp
	$(CXX) $(CXXFLAGS) -o $(SENDER_TARGET) rdt_sender.cpp

$(RECEIVER_TARGET): rdt_receiver.cpp rdt_common.hpp
	$(CXX) $(CXXFLAGS) -o $(RECEIVER_TARGET) rdt_receiver.cpp

clean:
	rm -f $(SENDER_TARGET) $(RECEIVER_TARGET)


