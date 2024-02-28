#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>   
#include <unistd.h> 
#include <ncurses.h>    // Pour l'interface utilisateur dans le terminal.
#include <sys/socket.h> // Pour les sockets.
#include <netinet/in.h> // Pour les constantes et structures relatives aux adresses Internet.
#include <arpa/inet.h>  // Pour les fonctions de manipulation d'adresses IP.
#include <pthread.h>    // Pour l'utilisation de threads.

#define BUFFER_SIZE 1024 // Définit la taille maximale des messages.

// Déclaration de deux fenêtres ncurses pour l'affichage des messages reçus et l'envoi de nouveaux messages.
WINDOW *win_send, *win_receive;

// Initialisation de l'interface utilisateur avec ncurses.
void init_ui(WINDOW **win_send, WINDOW **win_receive) {
    initscr();          // Initialise l'écran pour utiliser ncurses.
    cbreak();           // Désactive le buffering de ligne, permettant la lecture de chaque caractère dès qu'il est tapé.
    noecho();           // Ne montre pas les caractères tapés par l'utilisateur à l'écran.

    int height, width;
    getmaxyx(stdscr, height, width); // Récupère les dimensions du terminal.

    // Création des fenêtres pour afficher et envoyer des messages.
    *win_receive = newwin(height - 3, width, 0, 0);
    *win_send = newwin(3, width, height - 3, 0);

    // Configuration de la fenêtre de réception pour autoriser le défilement.
    scrollok(*win_receive, TRUE);
    // Affiche une bordure autour de la fenêtre d'envoi et rafraîchit les deux fenêtres pour les montrer à l'écran.
    box(*win_send, 0, 0);
    wrefresh(*win_send);
    wrefresh(*win_receive);
}

// Fonction exécutée par le thread de réception pour gérer les messages entrants.
void* handle_receive(void *args) {
    int sockfd = *((int*)args); // Convertit l'argument en un descripteur de socket.
    char buffer[BUFFER_SIZE];   // Buffer pour stocker les messages reçus.
    int n;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE); // Nettoie le buffer.
        n = recv(sockfd, buffer, BUFFER_SIZE, 0); // Réceptionne un message.
        if (n <= 0) { // Si recv retourne 0, la connexion est fermée; s'il retourne <0, une erreur est survenue.
            break;
        }
        buffer[n] = '\0'; // Assure que le message est correctement terminé par un caractère nul.
        // Affiche le message dans la fenêtre de réception et la rafraîchit.
        wprintw(win_receive, "%s\n", buffer);
        wrefresh(win_receive);
    }

    return NULL;
}

// Gère l'envoi de messages.
void handle_send(WINDOW *win_send, int sockfd, char* username) {
    char msg[BUFFER_SIZE];         // Pour stocker le message à envoyer.
    char msg_to_send[BUFFER_SIZE + 50]; // Pour stocker le message formaté avec le nom d'utilisateur.
    int index = 0; // Index pour construire le message caractère par caractère.
    int ch; // Pour stocker le caractère entré par l'utilisateur.

    echo(); // Active l'affichage des caractères tapés par l'utilisateur.
    wmove(win_send, 1, 10); // Positionne le curseur dans la fenêtre d'envoi.

    while (1) {
        ch = wgetch(win_send); // Attente de la saisie de l'utilisateur.

        if (ch == '\n') { // Si l'utilisateur appuie sur Entrée, envoie le message.
            msg[index] = '\0'; // Termine le message.
            // Formate le message avec le nom d'utilisateur et l'envoie.
            snprintf(msg_to_send, sizeof(msg_to_send), "%s : %s", username, msg);
            send(sockfd, msg_to_send, strlen(msg_to_send), 0);
            // Nettoie la fenêtre d'envoi et affiche à nouveau la bordure.
            wclear(win_send);
            box(win_send, 0, 0);
            wrefresh(win_send);
            index = 0; // Réinitialise l'index pour le prochain message.
        } else if (ch == KEY_BACKSPACE || ch == 127) { // Gère la suppression d'un caractère.
            if (index > 0) {
                index--;
                mvwdelch(win_send, 1, 10 + index); // Supprime le caractère à l'écran.
            }
        } else if (index < BUFFER_SIZE - 1) { // Assure que le message ne dépasse pas la taille du buffer.
            msg[index++] = (char)ch; // Ajoute le caractère au message et rafraîchit la fenêtre d'envoi.
            wrefresh(win_send);
        }
    }

    noecho(); // Désactive l'affichage des caractères tapés.
}


int main(int argc, char *argv[]) {
    // Vérifie que le bon nombre d'arguments est passé au programme.
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server IP> <port> <Pseudo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int sockfd; // Descripteur de socket.
    struct sockaddr_in serv_addr; // Structure contenant l'adresse du serveur.

    // Crée un socket TCP.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("#Erreur de création du Socket");
        return EXIT_FAILURE;
    }

    // Prépare l'adresse du serveur.
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // Utilise IPv4.
    serv_addr.sin_port = htons(atoi(argv[2])); // Convertit le numéro de port en réseau.

    // Convertit l'adresse IP en binaire et la stocke dans serv_addr.
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("#Adresse IP introuvable");
        return EXIT_FAILURE;
    }

    // Établit une connexion au serveur.
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("#Connection échoué");
        return EXIT_FAILURE;
    }

    // Envoie le nom d'utilisateur au serveur.
    send(sockfd, argv[3], strlen(argv[3]), 0);

    // Initialise l'interface utilisateur.
    init_ui(&win_send, &win_receive);

    // Crée un thread pour gérer la réception des messages.
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, handle_receive, (void*)&sockfd) != 0) {
        perror("#Création du thread échoué");
        endwin(); // Termine ncurses proprement.
        close(sockfd); // Ferme le socket.
        return EXIT_FAILURE;
    }

    // Gère l'envoi de messages.
    handle_send(win_send, sockfd, argv[3]);

    // Attend que le thread de réception se termine.
    pthread_join(receive_thread, NULL);

    // Termine ncurses et ferme le socket avant de quitter.
    endwin();
    close(sockfd);
    return 0;
}

