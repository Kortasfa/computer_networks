#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Простой TCP сервер, который принимает сообщения от клиента в формате:
//   <имя_клиента>\n<число_клиента>\n
// И отвечает:
//   <имя_сервера>\n<число_сервера>\n
// Если число клиента вне диапазона [1,100], сервер инициирует корректное завершение работы.

static volatile sig_atomic_t g_shutdown_requested = 0;

static void handle_sigusr1(int /*signum*/) {
    g_shutdown_requested = 1;
}

static void handle_sigchld(int /*signum*/) {
    // Забираем завершенные дочерние процессы, чтобы избежать зомби
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}

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
        if (n == 0) break; // соединение закрыто
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
    const char *server_name = "Server of Computer Networks";
    int server_number = 50;

    int port = 5555;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    // Устанавливаем обработчики сигналов
    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sigaction(SIGUSR1, &sa_usr1, NULL);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    printf("[server] Socket created\n");
    fflush(stdout);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    printf("[server] Bound to port %d\n", port);
    fflush(stdout);

    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }
    printf("[server] Listening...\n");
    fflush(stdout);

    while (!g_shutdown_requested) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int conn_fd = accept(listen_fd, (struct sockaddr *)&cliaddr, &clilen);
        if (conn_fd < 0) {
            if (errno == EINTR) {
                if (g_shutdown_requested) break;
                continue;
            }
            perror("accept");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(conn_fd);
            continue;
        }

        if (pid == 0) {
            // Дочерний процесс
            close(listen_fd);

            char buf[512];
            if (recv_until_two_newlines(conn_fd, buf, sizeof(buf)) < 0) {
                perror("recv");
                close(conn_fd);
                _exit(1);
            }

            // Парсим: первая строка = имя клиента, вторая строка = число
            char *first_nl = strchr(buf, '\n');
            char *second_line = NULL;
            if (first_nl) {
                *first_nl = '\0';
                second_line = first_nl + 1;
            }

            int client_number = 0;
            if (second_line) {
                client_number = atoi(second_line);
            }

            printf("[server] Client name: %s\n", buf[0] ? buf : "<empty>");
            printf("[server] Server name: %s\n", server_name);
            printf("[server] Client number: %d\n", client_number);
            printf("[server] Server number: %d\n", server_number);
            printf("[server] Sum: %d\n", client_number + server_number);
            fflush(stdout);

            // Подготавливаем ответ
            char resp[512];
            int n = snprintf(resp, sizeof(resp), "%s\n%d\n", server_name, server_number);
            if (n < 0 || n >= (int)sizeof(resp)) {
                close(conn_fd);
                _exit(1);
            }
            if (send_all(conn_fd, resp, (size_t)n) < 0) {
                perror("send");
                close(conn_fd);
                _exit(1);
            }

            if (second_line && (client_number < 1 || client_number > 100)) {
                pid_t ppid = getppid();
                kill(ppid, SIGUSR1);
            }

            close(conn_fd);
            _exit(0);
        } else {
            // Родительский процесс
            close(conn_fd);
        }
    }

    // Корректное завершение работы
    printf("[server] Shutting down...\n");
    fflush(stdout);
    close(listen_fd);
    // Позволяем дочерним процессам завершиться; даем немного времени и забираем оставшиеся
    sleep(1);
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    return 0;
}


