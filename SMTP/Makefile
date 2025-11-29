# Makefile для сборки и тестирования SMTP-клиента

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11
TARGET = smtp_client
SOURCE = smtp_client.cpp
TESTS_DIR = tests

# Цели по умолчанию
.PHONY: all clean test unit-test integration-test run help

all: $(TARGET)

# Компиляция основного проекта
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

# Компиляция unit-тестов
unit_tests: $(TESTS_DIR)/unit_tests.cpp $(TESTS_DIR)/test_framework.h
	$(CXX) $(CXXFLAGS) -I$(TESTS_DIR) -o $(TESTS_DIR)/unit_tests $(TESTS_DIR)/unit_tests.cpp

# Запуск unit-тестов
unit-test: unit_tests
	@echo "Running unit tests..."
	@cd $(TESTS_DIR) && ./unit_tests
	@rm -f $(TESTS_DIR)/unit_tests

# Запуск интеграционных тестов
integration-test: $(TARGET)
	@echo "Running integration tests..."
	@echo "Note: SMTP server must be running on localhost:25"
	@$(TESTS_DIR)/integration_tests.sh

# Запуск всех тестов
test: $(TARGET)
	@./run_all_tests.sh

# Очистка
clean:
	rm -f $(TARGET)
	rm -f $(TESTS_DIR)/unit_tests

# Запуск клиента
run: $(TARGET)
	./$(TARGET)

# Справка
help:
	@echo "Available targets:"
	@echo "  make              - Build the SMTP client"
	@echo "  make unit-test    - Compile and run unit tests"
	@echo "  make integration-test - Run integration tests (requires SMTP server)"
	@echo "  make test         - Run all tests"
	@echo "  make clean        - Remove compiled files"
	@echo "  make run          - Run the SMTP client"
	@echo "  make help         - Show this help message"
