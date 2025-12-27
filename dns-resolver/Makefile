CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
TARGET = dns_resolver
SOURCES = dns_resolver.cpp
HEADERS = dns_resolver.hpp

.PHONY: all clean run help

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

help:
	@echo "Available targets:"
	@echo "  make              - Build the DNS resolver"
	@echo "  make clean        - Remove compiled files"
	@echo "  make run          - Run the DNS resolver"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(TARGET) [-d] <domain> <type>"
	@echo "  Example: ./$(TARGET) example.com A"
	@echo "  Example: ./$(TARGET) -d google.com AAAA"

