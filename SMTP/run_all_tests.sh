#!/bin/bash
# Главный скрипт для запуска всех тестов SMTP-клиента

# Цвета
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BOLD='\033[1m'
NC='\033[0m'

PROJECT_DIR=$(pwd)
TESTS_DIR="$PROJECT_DIR/tests"

echo -e "${BOLD}${MAGENTA}========================================${NC}"
echo -e "${BOLD}${MAGENTA}     SMTP Client - Test Suite${NC}"
echo -e "${BOLD}${MAGENTA}========================================${NC}"
echo ""

# Проверка наличия директории с тестами
if [ ! -d "$TESTS_DIR" ]; then
    echo -e "${RED}✗ Tests directory not found: $TESTS_DIR${NC}"
    exit 1
fi

# Счётчики
TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0

# Функция для запуска набора тестов
run_test_suite() {
    local suite_name=$1
    local suite_command=$2
    
    echo -e "${BOLD}${CYAN}>>> Running: ${suite_name}${NC}"
    echo ""
    
    ((TOTAL_SUITES++))
    
    if eval "$suite_command"; then
        echo ""
        echo -e "${GREEN}${BOLD}✓ ${suite_name} completed successfully${NC}"
        ((PASSED_SUITES++))
    else
        echo ""
        echo -e "${RED}${BOLD}✗ ${suite_name} failed${NC}"
        ((FAILED_SUITES++))
    fi
    
    echo ""
    echo -e "${BLUE}----------------------------------------${NC}"
    echo ""
}

# 1. Проверка компиляции основного проекта
echo -e "${YELLOW}Step 1: Building main project...${NC}"
if make clean && make; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo ""
echo -e "${BLUE}----------------------------------------${NC}"
echo ""

# 2. Компиляция и запуск unit-тестов
echo -e "${YELLOW}Step 2: Compiling and running unit tests...${NC}"
if [ -f "$TESTS_DIR/unit_tests.cpp" ]; then
    cd "$TESTS_DIR"
    if g++ -Wall -Wextra -std=c++11 -I. -o unit_tests unit_tests.cpp; then
        echo -e "${GREEN}✓ Unit tests compiled${NC}"
        echo ""
        run_test_suite "Unit Tests" "./unit_tests"
        rm -f unit_tests
    else
        echo -e "${RED}✗ Unit tests compilation failed${NC}"
        ((TOTAL_SUITES++))
        ((FAILED_SUITES++))
    fi
    cd "$PROJECT_DIR"
else
    echo -e "${YELLOW}! Unit tests not found, skipping${NC}"
    echo ""
fi

# 3. Запуск интеграционных тестов
echo -e "${YELLOW}Step 3: Running integration tests...${NC}"
echo ""

if [ -f "$TESTS_DIR/integration_tests.sh" ]; then
    chmod +x "$TESTS_DIR/integration_tests.sh"
    
    # Проверка доступности SMTP-сервера
    if nc -z -w 2 localhost 25 2>/dev/null; then
        echo -e "${GREEN}✓ SMTP server detected${NC}"
        echo ""
        run_test_suite "Integration Tests" "$TESTS_DIR/integration_tests.sh"
    else
        echo -e "${YELLOW}! SMTP server not running${NC}"
        echo -e "${YELLOW}  Start server with: python3 -m smtpd -n -c DebuggingServer localhost:25${NC}"
        echo -e "${YELLOW}  Skipping integration tests${NC}"
        echo ""
    fi
else
    echo -e "${YELLOW}! Integration tests not found, skipping${NC}"
    echo ""
fi

# 4. Проверка синтаксиса и предупреждений
echo -e "${YELLOW}Step 4: Code quality checks...${NC}"
echo ""

echo -e "${CYAN}Checking for compiler warnings...${NC}"
if g++ -Wall -Wextra -Wpedantic -std=c++11 -fsyntax-only smtp_client.cpp 2>&1 | grep -i "warning\|error"; then
    echo -e "${YELLOW}! Found warnings or errors${NC}"
else
    echo -e "${GREEN}✓ No warnings or errors${NC}"
fi
echo ""

echo -e "${CYAN}Checking code statistics...${NC}"
echo "  Lines of code: $(wc -l < smtp_client.cpp)"
echo "  File size: $(ls -lh smtp_client.cpp | awk '{print $5}')"
echo ""

echo -e "${BLUE}----------------------------------------${NC}"
echo ""

# Итоговая статистика
echo -e "${BOLD}${MAGENTA}========================================${NC}"
echo -e "${BOLD}${MAGENTA}         Final Summary${NC}"
echo -e "${BOLD}${MAGENTA}========================================${NC}"
echo ""

if [ $TOTAL_SUITES -gt 0 ]; then
    echo "Test Suites:"
    echo "  Total:  $TOTAL_SUITES"
    echo -e "${GREEN}  Passed: $PASSED_SUITES${NC}"
    if [ $FAILED_SUITES -gt 0 ]; then
        echo -e "${RED}  Failed: $FAILED_SUITES${NC}"
    else
        echo "  Failed: $FAILED_SUITES"
    fi
    echo ""
fi

# Проверка исполняемого файла
if [ -f "./smtp_client" ]; then
    echo -e "${GREEN}✓ Executable ready: smtp_client${NC}"
    echo "  Size: $(ls -lh smtp_client | awk '{print $5}')"
else
    echo -e "${RED}✗ Executable not found${NC}"
fi

echo ""
echo -e "${BOLD}${MAGENTA}========================================${NC}"
echo ""

# Финальный результат
if [ $FAILED_SUITES -eq 0 ]; then
    echo -e "${GREEN}${BOLD}🎉 All tests passed! Project is ready.${NC}"
    echo ""
    echo -e "You can now run the SMTP client:"
    echo -e "  ${CYAN}./smtp_client localhost sender@test.com receiver@test.com \"Subject\" \"Body\"${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}${BOLD}⚠️  Some tests failed. Please review the errors above.${NC}"
    echo ""
    exit 1
fi
