#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>    
#include <unistd.h>    
#include <pthread.h>    // Pour l'utilisation de threads.
#include <netinet/in.h> // Pour les constantes et structures relatives aux adresses Internet.

#define PORT 4242                  // Port d'écoute du serveur.
#define BUFFER_SIZE 1024           // Taille maximale des messages.
#define MAX_CLIENTS 10             // Nombre maximal de clients simultanés.
#define MSG_CONNECT "#%s a rejoint la discussion"    // Message de connexion.
#define MSG_DISCONNECT "#%s a quitté la discussion"  // Message de déconnexion.

int client_sockets[MAX_CLIENTS];   // Tableau pour stocker les descripteurs de socket des clients.
int client_count = 0;              // Nombre actuel de clients connectés.
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; 

// Fonction pour diffuser un message à tous les clients connectés.
void broadcast_message(char *message) {
    // Ouverture du fichier Log.txt pour enregistrer les messages échangés.
    FILE *file = fopen("Log.txt", "a");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier Log.txt");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&client_mutex); // Verrouillage du mutex pour accéder à la liste des clients.
    for (int i = 0; i < client_count; i++) {
        send(client_sockets[i], message, strlen(message), 0); // Envoi du message à chaque client.
    }
    pthread_mutex_unlock(&client_mutex); // Déverrouillage du mutex.

    // Enregistrement du message dans le fichier Log.txt.
    fprintf(file, "\n%s", message);
    fclose(file); // Fermeture du fichier.
}

// Fonction exécutée par chaque thread pour gérer la communication avec un client.
void *client_handler(void *arg) {
    int sock = *(int *)arg; // Récupération du descripteur de socket du client.
    char buffer[BUFFER_SIZE]; // Buffer pour stocker les messages reçus.
    char username[BUFFER_SIZE]; // Pour stocker le nom d'utilisateur du client.
    int read_size; // Pour stocker le résultat de recv().

    // Réception du nom d'utilisateur du client.
    read_size = recv(sock, username, BUFFER_SIZE, 0);
    if (read_size > 0) {
        username[read_size] = '\0'; // Assure que la chaîne est terminée par un caractère nul.
        printf("#%s connecté\n", username); // Affiche un message dans la console du serveur.
              
        // Envoie un message à tous les clients indiquant qu'un nouveau client a rejoint.
        char welcome_message[BUFFER_SIZE + strlen(MSG_CONNECT) + 5];
        sprintf(welcome_message, MSG_CONNECT, username);
        broadcast_message(welcome_message);
    } else {
        return NULL; // En cas d'erreur, termine le thread.
    }

    // Boucle pour recevoir et diffuser les messages du client.
    while ((read_size = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0'; // Assure que le message est correctement terminé.
        broadcast_message(buffer); // Diffuse le message à tous les clients.
    }

    // Si le client se déconnecte correctement.
    if (read_size == 0) {
        printf("#%s déconnecté\n", username); // Affiche un message dans la console du serveur.
        // Envoie un message à tous les clients indiquant qu'un client a quitté.
        char disconnect_message[BUFFER_SIZE + strlen(MSG_DISCONNECT) + 5];
        sprintf(disconnect_message, MSG_DISCONNECT, username);
        broadcast_message(disconnect_message);

        // Retire le socket du client de la liste des clients connectés.
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
        perror("#recv failed"); // Affiche une erreur si recv échoue.
    }

    close(sock); // Ferme le socket du client.
    return NULL;
}

int main() {
    int server_fd, new_socket, c; // Descripteurs de socket pour le serveur et les clients.
    struct sockaddr_in server, client; // Structures pour les adresses IP du serveur et des clients.
    pthread_t thread_id; // Identifiant du thread pour chaque client.

    // Création du socket serveur.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("#Echec de la création du socket");
        return 1;
    }

    // Configuration de l'adresse du serveur.
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // Accepte les connexions sur toutes les interfaces.
    server.sin_port = htons(PORT); // Numéro de port.

    // Liaison du socket serveur à l'adresse configurée.
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("#Liaison échoué");
        return 1;
    }

    // Mise en écoute du serveur.
    listen(server_fd, 3);
    puts("#Prêt à recevoir des connections");
    c = sizeof(struct sockaddr_in);

    // Boucle pour accepter les connexions entrantes.
    while ((new_socket = accept(server_fd, (struct sockaddr *)&client, (socklen_t*)&c))) {
        puts("Connection accepté");

        pthread_mutex_lock(&client_mutex); // Verrouille le mutex pour modifier la liste des clients.
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = new_socket; // Ajoute le nouveau client à la liste.
            // Crée un nouveau thread pour gérer la communication avec ce client.
            if (pthread_create(&thread_id, NULL, client_handler, (void*)&new_socket) < 0) {
                perror("#Création du thread échoué");
                return 1;
            }
        } else {
            // Refuse la connexion si le nombre maximal de clients est atteint.
            char *message = "#Nombre maximum de clients atteint, veuillez réessayer plus tard\n";
            send(new_socket, message, strlen(message), 0);
            close(new_socket);
        }
        pthread_mutex_unlock(&client_mutex); // Déverrouille le mutex.
    }

    if (new_socket < 0) {
        perror("#Acceptation échouée");
        return 1;
    }

    close(server_fd); // Ferme le socket serveur à la fin du programme.
    return 0;
}

