#include "common.h"

/**
 * Functie care trimite un pachet catre server.
 * @param sockfd - socketul pe care se trimite pachetul
*/
int send_to_server (int sockfd) {
    char buff[BUFFLEN];
    memset(buff, 0, BUFFLEN);

    fgets(buff, BUFFLEN, stdin);
    char *token = strtok(buff, " ");

    struct packet_from_TCP packet;
    packet.SF = 2; // for the unsubscribe case
    int stop = 0; // for exit case

    if (strncmp(token, "subscribe", 9) == 0) {

        token = strtok(NULL, " ");
        strcpy(packet.topic, token);
        token = strtok(NULL, " ");
        packet.SF = atoi(token);
        strcpy(packet.action_type, "sub");
    
    } else if (strncmp(token, "unsubscribe", 11) == 0) {

        token = strtok(NULL, " ");
        strcpy(packet.topic, token);
        strcpy(packet.action_type, "unsub");
    
    } else if (strncmp(token, "exit", 4) == 0) {

        strcpy(packet.topic, "exit");
        strcpy(packet.action_type, "exit");
        stop = 1;
    
    } else {
        // Comanda invalida
        return -1;
    }
    
    int ret = send(sockfd, &packet, sizeof(struct packet_from_TCP), 0);
    if (ret < 0) {
        fprintf(stderr, "Error: send\n");
    }

    if (stop) {
        return -1;
    }
    return 0;
}

/**
 * Functie care primeste un pachet de la server.
 * @param sockfd - socketul de pe care se primeste pachetul
*/
int receive_from_server(int sockfd) {

    struct packet_to_TCP packet;
    int ret = recv(sockfd, (char *)&packet, sizeof(struct packet_to_TCP), 0);
    if (ret < 0) {
        fprintf(stderr, "Error: recv\n");
        return -1;
    }

    if (ret == 0) {
        //Severul s-a inchis.
        close(sockfd);
        return -1;
    }

    if (strcmp(packet.type, "error") == 0) {
        //Afisare eroare pentru comenzile invalide de la clientul TCP.
        fprintf(stderr, "Error: %s\n", packet.data);
        return 0;
    
    } else if (strcmp(packet.type, "success") == 0) {

        if (strcmp(packet.data, "sub") == 0) {
            cout<<"Subscribed to topic."<<endl;
        } else if (strcmp(packet.data, "unsub") == 0) {
            cout<<"Unsubscribed from topic."<<endl;
        }
        return 0;

    } else if (strncmp(packet.type, "exit", 4) == 0) {

        close(sockfd);
        return -1;
    }

    cout<<packet.ip<<":"<<packet.port<<" - "<<packet.topic<<" - "<<packet.type<<" - "<<packet.data<<endl;
    return 0;
}

int main(int argc, char **argv) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int ret;
    int sockfd;
    struct sockaddr_in serv_addr;

    if (argc < 4) {
        fprintf(stderr, "Not enough argumennts\n");
        exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error: socket\n");
        exit(0);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    if (ret == 0) {
        fprintf(stderr, "Error: inet_aton\n");
        exit(0);
    }

    ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        fprintf(stderr, "Error: connect\n");
        exit(0);
    }

    // Trimit serverului id-ul clientului.
    char buff[BUFFLEN];
    memset(buff, 0, BUFFLEN);
    strcpy(buff, argv[1]);
    int size = strlen(buff);
    ret = send(sockfd, buff, size, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: send\n");
        exit(0);
    }

    vector<struct pollfd> pollfds;
    pollfds.push_back({STDIN_FILENO, POLLIN, 0});
    pollfds.push_back({sockfd, POLLIN, 0});

    while (1) {
        ret = poll(pollfds.data(), pollfds.size(), -1);
        if (ret < 0) {
            fprintf(stderr, "Error: poll\n");
            break;
        }

        if (pollfds[1].revents & POLLIN) {
            // primesc de la server
            ret = receive_from_server(sockfd);
            if (ret < 0) {
                break;
            }
        } else if (pollfds[0].revents & POLLIN) {
            // trimit serverului
            ret = send_to_server(sockfd);
            if (ret < 0) {
                close(sockfd);
                break;
            }
        }
    }
    return 0;
}