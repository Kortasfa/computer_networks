#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

static int recv_until_two_newlines(int fd, char *out, size_t out_cap) {
    size_t used = 0;
    int newline_count = 0;
    while (used + 1 < out_cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        out[used++] = c;
        if (c == '\n') {
            newline_count++;
            if (newline_count >= 2) break;
        }
    }
    out[used] = '\0';
    return 0;
}

int main(int argc, char **argv) {
    const char *server_ip = "127.0.0.1";
    int port = 5555;
    const char *client_name = "Client of Computer Networks";

    if (argc >= 2) client_name = argv[1];
    if (argc >= 3) server_ip = argv[2];
    if (argc >= 4) {
        port = atoi(argv[3]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[3]);l
            return 1;
        }
    }

    // 1) Читаем целое число из stdin [1..100]
    int user_number = 0;
    if (scanf("%d", &user_number) != 1) {
        fprintf(stderr, "Failed to read integer from stdin\n");
        return 1;
    }

    // 2) Подготавливаем сообщение: имя + число в отдельных строках
    char msg[512];
    int m = snprintf(msg, sizeof(msg), "%s\n%d\n", client_name, user_number);
    if (m < 0 || m >= (int)sizeof(msg)) {
        fprintf(stderr, "Message too long\n");
        return 1;
    }

    // 3) Устанавливаем TCP соединение
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    printf("[client] Socket created\n");
    fflush(stdout);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server ip: %s\n", server_ip);
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }
    printf("[client] Connected to %s:%d\n", server_ip, port);
    fflush(stdout);

    // 4) Отправляем сообщение
    if (send_all(fd, msg, (size_t)m) < 0) {
        perror("send");
        close(fd);
        return 1;
    }
    printf("[client] Sent name and number\n");
    fflush(stdout);

    // 5) Получаем ответ
    char resp[512];
    if (recv_until_two_newlines(fd, resp, sizeof(resp)) < 0) {
        perror("recv");
        close(fd);
        return 1;
    }

    // 6) Парсим ответ
    char *first_nl = strchr(resp, '\n');
    char *second_line = NULL;
    if (first_nl) {
        *first_nl = '\0';
        second_line = first_nl + 1;
    }
    int server_number = 0;
    if (second_line) {
        server_number = atoi(second_line);
    }

    printf("[client] Client name: %s\n", client_name);
    printf("[client] Server name: %s\n", resp[0] ? resp : "<empty>");
    printf("[client] Client number: %d\n", user_number);
    printf("[client] Server number: %d\n", server_number);
    printf("[client] Sum: %d\n", user_number + server_number);
    fflush(stdout);

    // 8) Закрываем соединение
    close(fd);
    return 0;
}


