#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
SMTP_SERVER="127.0.0.1"
SMTP_PORT=2525

echo -e "${BOLD}${BLUE}=====================================${NC}"
echo -e "${BOLD}${BLUE}  SMTP Client - Integration Tests${NC}"
echo -e "${BOLD}${BLUE}=====================================${NC}"
echo ""

if [ ! -f "./smtp_client" ]; then
    echo -e "${RED}✗ Error: smtp_client executable not found${NC}"
    echo "Run: make"
    exit 1
fi

echo -e "${GREEN}✓ Found smtp_client executable${NC}"
echo ""

check_smtp_server() {
    echo -e "${CYAN}Checking SMTP server on ${SMTP_SERVER}:${SMTP_PORT}...${NC}"
    
    if nc -z -w 2 ${SMTP_SERVER} ${SMTP_PORT} 2>/dev/null; then
        echo -e "${GREEN}✓ SMTP server is running${NC}"
        return 0
    else
        echo -e "${RED}✗ SMTP server is not accessible${NC}"
        echo ""
        echo -e "${YELLOW}Please start SMTP server first:${NC}"
        echo "  python3 -m smtpd -n -c DebuggingServer 127.0.0.1:2525"
        echo ""
        return 1
    fi
}

run_test() {
    local test_name=$1
    local from=$2
    local to=$3
    local subject=$4
    local body=$5
    
    echo -e "${CYAN}Running: ${test_name}${NC}"
    
    output=$(./smtp_client ${SMTP_SERVER} "${from}" "${to}" "${subject}" "${body}" 2>&1)
    exit_code=$?
    
    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "Письмо успешно отправлено"; then
        echo -e "${GREEN}  ✓ PASSED${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}  ✗ FAILED${NC}"
        echo "  Exit code: $exit_code"
        echo "  Output:"
        echo "$output" | sed 's/^/    /'
        ((TESTS_FAILED++))
        return 1
    fi
}

run_error_test() {
    local test_name=$1
    local server=$2
    local from=$3
    local to=$4
    local subject=$5
    local body=$6
    
    echo -e "${CYAN}Running: ${test_name}${NC}"
    
    output=$(./smtp_client "${server}" "${from}" "${to}" "${subject}" "${body}" 2>&1)
    exit_code=$?
    
    if [ $exit_code -ne 0 ]; then
        echo -e "${GREEN}  ✓ PASSED (error expected)${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}  ✗ FAILED (expected error but succeeded)${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

if ! check_smtp_server; then
    echo -e "${YELLOW}Skipping integration tests (server not available)${NC}"
    exit 0
fi

echo ""
echo -e "${YELLOW}Basic Functionality Tests:${NC}"

run_test "Simple text email" \
    "test@example.com" \
    "recipient@example.com" \
    "Test Subject" \
    "This is a test message"

echo ""

run_test "Email with long subject" \
    "sender@test.com" \
    "receiver@test.com" \
    "This is a very long subject line that contains multiple words" \
    "Simple body"

echo ""

run_test "Email with multiline body" \
    "admin@company.com" \
    "user@client.com" \
    "Multiline Test" \
    "Line 1
Line 2
Line 3"

echo ""

run_test "Email with special characters" \
    "user@test.com" \
    "recipient@test.com" \
    "Special chars: !@#$%" \
    "Body with chars: & * () [] {}"

echo ""

run_test "Email with empty subject" \
    "sender@example.com" \
    "receiver@example.com" \
    "" \
    "Body without subject"

echo ""

run_test "Very short email" \
    "a@b.com" \
    "c@d.com" \
    "Hi" \
    "OK"

echo ""

echo -e "${YELLOW}Error Handling Tests:${NC}"

run_error_test "Invalid SMTP server" \
    "nonexistent.server.invalid" \
    "test@test.com" \
    "user@user.com" \
    "Test" \
    "Test"

echo ""

run_error_test "Connection refused" \
    "127.0.0.1" \
    "test@test.com" \
    "user@user.com" \
    "Test" \
    "Test" 

echo ""

echo -e "${YELLOW}Edge Cases Tests:${NC}"

long_body=$(printf 'A%.0s' {1..1000})
run_test "Email with long body (1000 chars)" \
    "sender@test.com" \
    "receiver@test.com" \
    "Long message" \
    "$long_body"

echo ""

run_test "Email addresses with numbers" \
    "user123@test456.com" \
    "recipient789@domain012.com" \
    "Numbers test" \
    "Testing email with numbers"

echo ""

echo -e "${BOLD}======================================${NC}"
echo -e "${BOLD}Test Summary:${NC}"
echo "  Total:  $((TESTS_PASSED + TESTS_FAILED))"
echo -e "${GREEN}  Passed: ${TESTS_PASSED}${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}  Failed: ${TESTS_FAILED}${NC}"
else
    echo "  Failed: ${TESTS_FAILED}"
fi
echo -e "${BOLD}======================================${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}${BOLD}All integration tests passed! ✓${NC}"
    exit 0
else
    echo -e "${RED}${BOLD}Some integration tests failed! ✗${NC}"
    exit 1
fi
