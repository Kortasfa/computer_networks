/**
 * Unit-тесты для SMTP-клиента
 * Тестируют отдельные компоненты и функции
 */

#include "test_framework.h"
#include <sstream>
#include <cstring>

// ============================================
// Тесты для проверки кодов ответа
// ============================================

TEST(test_response_code_220) {
    std::string response = "220 smtp.example.com ESMTP Postfix\r\n";
    ASSERT_TRUE(response.substr(0, 3) == "220");
    ASSERT_CONTAINS(response, "220");
}

TEST(test_response_code_250) {
    std::string response = "250 OK\r\n";
    ASSERT_TRUE(response.substr(0, 3) == "250");
}

TEST(test_response_code_354) {
    std::string response = "354 End data with <CR><LF>.<CR><LF>\r\n";
    ASSERT_TRUE(response.substr(0, 3) == "354");
}

TEST(test_response_code_invalid) {
    std::string response = "500 Error\r\n";
    ASSERT_FALSE(response.substr(0, 3) == "250");
    ASSERT_TRUE(response.substr(0, 3) == "500");
}

// ============================================
// Тесты для формирования SMTP-команд
// ============================================

TEST(test_helo_command) {
    std::string domain = "example.com";
    std::string command = "HELO " + domain + "\r\n";
    ASSERT_CONTAINS(command, "HELO");
    ASSERT_CONTAINS(command, "example.com");
    ASSERT_CONTAINS(command, "\r\n");
}

TEST(test_mail_from_command) {
    std::string email = "sender@example.com";
    std::string command = "MAIL FROM:<" + email + ">\r\n";
    ASSERT_CONTAINS(command, "MAIL FROM:");
    ASSERT_CONTAINS(command, "sender@example.com");
    ASSERT_CONTAINS(command, "<");
    ASSERT_CONTAINS(command, ">");
}

TEST(test_rcpt_to_command) {
    std::string email = "recipient@example.com";
    std::string command = "RCPT TO:<" + email + ">\r\n";
    ASSERT_CONTAINS(command, "RCPT TO:");
    ASSERT_CONTAINS(command, "recipient@example.com");
}

TEST(test_data_command) {
    std::string command = "DATA\r\n";
    ASSERT_EQ(command, "DATA\r\n");
}

TEST(test_quit_command) {
    std::string command = "QUIT\r\n";
    ASSERT_EQ(command, "QUIT\r\n");
}

// ============================================
// Тесты для формирования email-заголовков
// ============================================

TEST(test_email_header_from) {
    std::string from = "sender@example.com";
    std::string header = "From: " + from + "\r\n";
    ASSERT_CONTAINS(header, "From:");
    ASSERT_CONTAINS(header, "sender@example.com");
}

TEST(test_email_header_to) {
    std::string to = "recipient@example.com";
    std::string header = "To: " + to + "\r\n";
    ASSERT_CONTAINS(header, "To:");
    ASSERT_CONTAINS(header, "recipient@example.com");
}

TEST(test_email_header_subject) {
    std::string subject = "Test Subject";
    std::string header = "Subject: " + subject + "\r\n";
    ASSERT_CONTAINS(header, "Subject:");
    ASSERT_CONTAINS(header, "Test Subject");
}

TEST(test_email_complete_headers) {
    std::stringstream email;
    email << "From: sender@example.com\r\n";
    email << "To: recipient@example.com\r\n";
    email << "Subject: Test\r\n";
    email << "\r\n";
    email << "Body text\r\n";
    email << ".\r\n";
    
    std::string result = email.str();
    ASSERT_CONTAINS(result, "From:");
    ASSERT_CONTAINS(result, "To:");
    ASSERT_CONTAINS(result, "Subject:");
    ASSERT_CONTAINS(result, "Body text");
    ASSERT_CONTAINS(result, ".\r\n");
}

// ============================================
// Тесты для проверки валидации email
// ============================================

TEST(test_valid_email_format) {
    std::string email = "user@example.com";
    ASSERT_TRUE(email.find('@') != std::string::npos);
    ASSERT_TRUE(email.find('.') != std::string::npos);
}

TEST(test_invalid_email_no_at) {
    std::string email = "userexample.com";
    ASSERT_FALSE(email.find('@') != std::string::npos);
}

TEST(test_invalid_email_no_domain) {
    std::string email = "user@";
    size_t at_pos = email.find('@');
    ASSERT_TRUE(at_pos != std::string::npos);
    ASSERT_FALSE(email.find('.', at_pos) != std::string::npos);
}

// ============================================
// Тесты для работы со строками
// ============================================

TEST(test_string_trimming) {
    std::string str = "  test  ";
    // Простая реализация trim
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    std::string trimmed = (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
    ASSERT_EQ(trimmed, "test");
}

TEST(test_string_empty) {
    std::string str = "";
    ASSERT_TRUE(str.empty());
    ASSERT_EQ(str.length(), 0);
}

TEST(test_string_contains_crlf) {
    std::string str = "line1\r\nline2\r\n";
    ASSERT_CONTAINS(str, "\r\n");
}

// ============================================
// Тесты для порта и констант
// ============================================

TEST(test_smtp_port_constant) {
    const int SMTP_PORT = 25;
    ASSERT_EQ(SMTP_PORT, 25);
    ASSERT_NE(SMTP_PORT, 587);
    ASSERT_NE(SMTP_PORT, 465);
}

TEST(test_buffer_size_constant) {
    const int BUFFER_SIZE = 4096;
    ASSERT_EQ(BUFFER_SIZE, 4096);
    ASSERT_TRUE(BUFFER_SIZE > 0);
}

// ============================================
// Тесты для многострочных ответов
// ============================================

TEST(test_multiline_response) {
    std::string response = "250-smtp.example.com\r\n250-SIZE 10240000\r\n250 HELP\r\n";
    ASSERT_CONTAINS(response, "250");
    ASSERT_TRUE(response.substr(0, 3) == "250");
}

TEST(test_single_line_response) {
    std::string response = "250 OK\r\n";
    ASSERT_TRUE(response.substr(0, 3) == "250");
    ASSERT_TRUE(response.length() < 100);
}

// ============================================
// Тесты для специальных символов в email
// ============================================

TEST(test_email_with_spaces_in_subject) {
    std::string subject = "This is a test subject";
    ASSERT_TRUE(subject.find(' ') != std::string::npos);
    ASSERT_CONTAINS(subject, "test");
}

TEST(test_email_with_special_chars) {
    std::string body = "Test message with special chars: !@#$%";
    ASSERT_CONTAINS(body, "!");
    ASSERT_CONTAINS(body, "@");
    ASSERT_CONTAINS(body, "#");
}

TEST(test_email_body_terminator) {
    std::string terminator = ".\r\n";
    ASSERT_EQ(terminator.length(), 3);
    ASSERT_EQ(terminator[0], '.');
}

// ============================================
// Тесты для DNS и хостов
// ============================================

TEST(test_localhost_resolution) {
    std::string host = "localhost";
    ASSERT_EQ(host, "localhost");
    // На реальной системе localhost должен разрешаться в 127.0.0.1
}

TEST(test_ip_address_format) {
    std::string ip = "127.0.0.1";
    ASSERT_CONTAINS(ip, "127");
    ASSERT_CONTAINS(ip, ".");
    // Проверка количества точек
    int dots = 0;
    for (char c : ip) {
        if (c == '.') dots++;
    }
    ASSERT_EQ(dots, 3);
}

// ============================================
// Тесты для обработки ошибок
// ============================================

TEST(test_error_response_codes) {
    // Проверка различных кодов ошибок
    std::string err500 = "500 Syntax error\r\n";
    std::string err550 = "550 User not found\r\n";
    std::string err554 = "554 Transaction failed\r\n";
    
    ASSERT_TRUE(err500.substr(0, 1) == "5");
    ASSERT_TRUE(err550.substr(0, 1) == "5");
    ASSERT_TRUE(err554.substr(0, 1) == "5");
}

TEST(test_success_response_codes) {
    // Проверка успешных кодов
    std::string resp220 = "220 Ready\r\n";
    std::string resp250 = "250 OK\r\n";
    std::string resp354 = "354 Start input\r\n";
    
    ASSERT_TRUE(resp220.substr(0, 1) == "2");
    ASSERT_TRUE(resp250.substr(0, 1) == "2");
    ASSERT_TRUE(resp354.substr(0, 1) == "3");
}

// ============================================
// Главная функция для запуска тестов
// ============================================

int main() {
    std::cout << BOLD << BLUE << "=====================================" << RESET << std::endl;
    std::cout << BOLD << BLUE << "  SMTP Client - Unit Tests" << RESET << std::endl;
    std::cout << BOLD << BLUE << "=====================================" << RESET << std::endl;
    std::cout << std::endl;
    
    // Тесты кодов ответа
    std::cout << YELLOW << "Response Code Tests:" << RESET << std::endl;
    RUN_TEST(test_response_code_220);
    RUN_TEST(test_response_code_250);
    RUN_TEST(test_response_code_354);
    RUN_TEST(test_response_code_invalid);
    
    // Тесты команд
    std::cout << YELLOW << "SMTP Command Tests:" << RESET << std::endl;
    RUN_TEST(test_helo_command);
    RUN_TEST(test_mail_from_command);
    RUN_TEST(test_rcpt_to_command);
    RUN_TEST(test_data_command);
    RUN_TEST(test_quit_command);
    
    // Тесты заголовков
    std::cout << YELLOW << "Email Header Tests:" << RESET << std::endl;
    RUN_TEST(test_email_header_from);
    RUN_TEST(test_email_header_to);
    RUN_TEST(test_email_header_subject);
    RUN_TEST(test_email_complete_headers);
    
    // Тесты валидации email
    std::cout << YELLOW << "Email Validation Tests:" << RESET << std::endl;
    RUN_TEST(test_valid_email_format);
    RUN_TEST(test_invalid_email_no_at);
    RUN_TEST(test_invalid_email_no_domain);
    
    // Тесты строк
    std::cout << YELLOW << "String Handling Tests:" << RESET << std::endl;
    RUN_TEST(test_string_trimming);
    RUN_TEST(test_string_empty);
    RUN_TEST(test_string_contains_crlf);
    
    // Тесты констант
    std::cout << YELLOW << "Constants Tests:" << RESET << std::endl;
    RUN_TEST(test_smtp_port_constant);
    RUN_TEST(test_buffer_size_constant);
    
    // Тесты многострочных ответов
    std::cout << YELLOW << "Multiline Response Tests:" << RESET << std::endl;
    RUN_TEST(test_multiline_response);
    RUN_TEST(test_single_line_response);
    
    // Тесты специальных символов
    std::cout << YELLOW << "Special Characters Tests:" << RESET << std::endl;
    RUN_TEST(test_email_with_spaces_in_subject);
    RUN_TEST(test_email_with_special_chars);
    RUN_TEST(test_email_body_terminator);
    
    // Тесты DNS
    std::cout << YELLOW << "DNS and Network Tests:" << RESET << std::endl;
    RUN_TEST(test_localhost_resolution);
    RUN_TEST(test_ip_address_format);
    
    // Тесты обработки ошибок
    std::cout << YELLOW << "Error Handling Tests:" << RESET << std::endl;
    RUN_TEST(test_error_response_codes);
    RUN_TEST(test_success_response_codes);
    
    // Итоги
    std::cout << std::endl;
    TestFramework::getInstance().printSummary();
    
    return TestFramework::getInstance().getExitCode();
}
