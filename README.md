# Projet My_Teams ESIEE-IT
 
### Fichier : 
- client.c : Script du client qui envoie les messages
- server.c : Script du server qui reçoit et affiches les messages

### GCC : 
- server : `gcc server.c -o server -pthread`
- client : `gcc client.c -lncurses -o client`

### Utilisation : 
- server : `./server` 
- client : `./client <Adresse IP du server> <Port> <Pseudo>` par exemple --> `./client 127.0.0.1 4242 "Client1"`

### Log : 
- Log.txt : Une fois que le server à redirige un message vers un client il écrit un log du message dans un fichier log.txt.  
