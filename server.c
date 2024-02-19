#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

#define PORT 4242
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define MSG_CONNECT "#%s a rejoint la discussion"
#define MSG_DISCONNECT "#%s a quitté la discussion"

int client_sockets[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(char *message) {
    // Ouverture du fichier Log.txt en mode append
    FILE *file = fopen("Log.txt", "a");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier Log.txt");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        send(client_sockets[i], message, strlen(message), 0);
    }
    pthread_mutex_unlock(&client_mutex);

    // Écriture du message dans le fichier Log.txt
    fprintf(file, "\n%s", message);

    // Fermeture du fichier
    fclose(file);
}

void *client_handler(void *arg) {
    int sock = *(int *)arg;
    char buffer[BUFFER_SIZE];
    char username[BUFFER_SIZE];
    int read_size;

    read_size = recv(sock, username, BUFFER_SIZE, 0);
    if (read_size > 0) {
        username[read_size] = '\0';
        printf("#%s connecté\n", username);
              
        char welcome_message[BUFFER_SIZE + strlen(MSG_CONNECT) + 5];
        sprintf(welcome_message, MSG_CONNECT, username);
        broadcast_message(welcome_message);
    } else {
        return NULL;
    }

    while ((read_size = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        broadcast_message(buffer);
    }

    if (read_size == 0) {
        printf("#%s déconnecté\n", username);
        char disconnect_message[BUFFER_SIZE + strlen(MSG_DISCONNECT) + 5];
        sprintf(disconnect_message, MSG_DISCONNECT, username);
        broadcast_message(disconnect_message);

        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] == sock) {
                while (i < client_count - 1) {
                    client_sockets[i] = client_sockets[i + 1];
                    i++;
                }
                client_count--;
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);
    } else if (read_size == -1) {
        perror("#recv failed");
    }

    close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket, c;
    struct sockaddr_in server, client;
    pthread_t thread_id;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("#Echec de la création du socket");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("#Liaison échoué");
        return 1;
    }

    listen(server_fd, 3);
    puts("#Prêt à recevoir des connections");
    c = sizeof(struct sockaddr_in);
    while ((new_socket = accept(server_fd, (struct sockaddr *)&client, (socklen_t*)&c))) {
        puts("Connection accepté");

        pthread_mutex_lock(&client_mutex);
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = new_socket;
            if (pthread_create(&thread_id, NULL, client_handler, (void*)&new_socket) < 0) {
                perror("#Création du thread échoué");
                return 1;
            }
        } else {
            char *message = "#Nombre maximum de clients atteint, veuillez réessayer plus tard\n";
            send(new_socket, message, strlen(message), 0);
            close(new_socket);
        }
        pthread_mutex_unlock(&client_mutex);
    }

    if (new_socket < 0) {
        perror("#Acceptation échouée");
        return 1;
    }

    close(server_fd);
    return 0;
}

