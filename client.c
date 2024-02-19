#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

WINDOW *win_send, *win_receive;

void init_ui(WINDOW **win_send, WINDOW **win_receive) {
    initscr();
    cbreak();
    noecho();

    int height, width;
    getmaxyx(stdscr, height, width);

    *win_receive = newwin(height - 3, width, 0, 0);
    *win_send = newwin(3, width, height - 3, 0);

    scrollok(*win_receive, TRUE);
    box(*win_send, 0, 0);
    wrefresh(*win_send);
    wrefresh(*win_receive);
}

void* handle_receive(void *args) {
    int sockfd = *((int*)args);
    char buffer[BUFFER_SIZE];
    int n;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            break;
        }
        buffer[n] = '\0';
        wprintw(win_receive, "%s\n", buffer);
        wrefresh(win_receive);
    }

    return NULL;
}

void handle_send(WINDOW *win_send, int sockfd, char* username) {
    char msg[BUFFER_SIZE];
    char msg_to_send[BUFFER_SIZE + 50];
    int index = 0;
    int ch;

    echo();
    wmove(win_send, 1, 10);

    while (1) {
        ch = wgetch(win_send);

        if (ch == '\n') {
            msg[index] = '\0';
            snprintf(msg_to_send, sizeof(msg_to_send), "%s : %s", username, msg);
            send(sockfd, msg_to_send, strlen(msg_to_send), 0);
            wclear(win_send);
            box(win_send, 0, 0);
            wrefresh(win_send);
            index = 0;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (index > 0) {
                index--;
                mvwdelch(win_send, 1, 10 + index);
            }
        } else if (index < BUFFER_SIZE - 1) {
            msg[index++] = (char)ch;
            wrefresh(win_send);
        }
    }

    noecho();
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server IP> <port> <Pseudo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("#Erreur de création du Socket");
        return EXIT_FAILURE;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("#Adresse IP introuvable");
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("#Connection échoué");
        return EXIT_FAILURE;
    }

    send(sockfd, argv[3], strlen(argv[3]), 0);

    init_ui(&win_send, &win_receive);

    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, handle_receive, (void*)&sockfd) != 0) {
        perror("#Création du thread échoué");
        endwin();
        close(sockfd);
        return EXIT_FAILURE;
    }

    handle_send(win_send, sockfd, argv[3]);

    pthread_join(receive_thread, NULL);

    endwin();
    close(sockfd);
    return 0;
}

