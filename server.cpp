#include "common.h"

using namespace std;

/**
    Functie pentru inchiderea conexiunii unui singur client.
    @param sockfd - socket-ul clientului
    @param registered_clients - map-ul cu toti clientii inregistrati
*/
void close_client (int sockfd, unordered_map<string, struct client>  &registered_clients) {
    if (registered_clients.size() == 0) {
        return;
    }
    //Caut clinetul in map-ul de clienti.
    for(auto &it : registered_clients) {
        if (it.second.sockfd == sockfd) {
            if (it.second.connected) {
                //Daca este conectat, il deconectez.
                it.second.connected = false;
                cout<<"Client "<<it.second.id<<" disconnected.\n";
                return;
            }
        }
    }
}

/**
    Functie pentru inchiderea conexiunii tuturor clientilor.
    @param registered_clients - map-ul cu toti clientii inregistrati
*/
void close_connections(unordered_map<string, struct client>  &registered_clients) {

    if (registered_clients.size() == 0) {
        return;
    }
    for(auto &it : registered_clients) {
        if (it.second.connected) {
            //Deconectez clientul si ii trimit un pachet de exit.

            struct packet_to_TCP packet;
            strcpy(packet.ip, "0");
            packet.port = 0;
            strcpy(packet.topic, "0");
            strcpy(packet.type, "exit");
            strcpy(packet.data, "0");

            int ret = send(it.second.sockfd, &packet, sizeof(struct packet_to_TCP), 0);
            if(ret < 0) {
                fprintf(stderr, "Error sending exit packet to client.\n");
            }

            close(it.second.sockfd);
        }
    }
}

/**
 * Functie pentru verificarea unui client.
 * @param socket - socket-ul clientului
 * @param buffer - buffer-ul in care se afla mesajul id-ul primit de la client
 * @param registered_clients - map-ul cu toti clientii inregistrati
 * @param cli_addr - structura sockaddr_in a clientului
*/

int verify_client(int socket, char *buffer, unordered_map<string, struct client> &registered_clients,
                    struct sockaddr_in cli_addr) {
    char *token = strtok(buffer, " ");
    string id = token;

    if (registered_clients.find(id) == registered_clients.end()) {
        //Id-ul nu a fost gasit in map, deci se inregistreaza un client nou.

        struct client new_client;
        new_client.id = id;
        new_client.connected = true;
        new_client.port = ntohs(cli_addr.sin_port);
        new_client.ip = inet_ntoa(cli_addr.sin_addr);
        new_client.sockfd = socket;
        registered_clients[id] = new_client;

        cout<<"New client "<< id << " connected from "<< inet_ntoa(cli_addr.sin_addr) <<":"<< ntohs(cli_addr.sin_port)<<"." << endl;
        return 1;
    } else {
        if (registered_clients[id].connected == true) {
            //Clientul a fost gasit in map si este deja conectat.

            cout << "Client "<< id << " already connected." << endl;
            struct packet_to_TCP packet;
            strcpy(packet.type, "exit");

            int ret = send(socket, &packet, sizeof(struct packet_to_TCP), 0);
            if (ret < 0) {
                fprintf(stderr, "Error sending exit packet to client.\n");
            }

            return -1;
        } else {
            //Clientul a fost gasit in map, dar nu este conectat, deci se reconecteaza.

            registered_clients[id].connected = true;
            registered_clients[id].sockfd = socket;
            
            // Trimit pachetele care au fost trimise cat timp clientul era deconectat.
            for (uint i = 0; i < registered_clients[id].waiting_topics.size(); i++) {
                struct packet_to_TCP packet = registered_clients[id].waiting_topics[i];
                int ret = send(socket, &packet, sizeof(struct packet_to_TCP), 0);
                if (ret < 0) {
                    fprintf(stderr, "Error sending packet to client.\n");
                }
            }
            registered_clients[id].waiting_topics.clear();
            cout<<"New client "<< id << " connected from "<< inet_ntoa(cli_addr.sin_addr) <<":"<< ntohs(cli_addr.sin_port)<<"." << endl;
            return 1;
        }
    }
}

/**
 * Functie pentru construirea unui pachet care trebuie trimis unui client TCP.
 * @param packet - pachetul primit de la clientul UDP
 * @param ip - ip-ul clientului TCP
 * @param port - port-ul clientului TCP
*/

struct packet_to_TCP construct_TCP_packet(struct packet_from_UDP packet, string ip, int port) {
    
    struct packet_to_TCP packet_to_send;
    strcpy(packet_to_send.ip, ip.c_str());
    packet_to_send.port = port;

    strcpy(packet_to_send.topic, packet.topic);
    uint32_t num;
    double num_double;

    switch(packet.type) {
        case 0:
            // INT case
            strcpy(packet_to_send.type, "INT");
            num = ntohl(*(uint32_t *)(packet.data + 1));
            if (packet.data[0] == 1) {
                num = (-1) * num;
            } 
            sprintf(packet_to_send.data, "%d", num);
            break;
        case 1:
            // SHORT_REAL case
            strcpy(packet_to_send.type, "SHORT_REAL");
            num_double = ntohs(*(uint16_t *)(packet.data));
            num_double = num_double / 100;
            sprintf(packet_to_send.data, "%.2f", num_double);
            break;
        case 2:
            // FLOAT case
            strcpy(packet_to_send.type, "FLOAT");
            uint8_t power;

            num_double = ntohl(*(uint32_t *)(packet.data + 1));
            power = packet.data[5];

            for (uint8_t i = 0; i < power; i++) {
                num_double = num_double / 10;
            }
            if (packet.data[0] == 1) {
                num_double = (-1) * num_double;
            }
            sprintf(packet_to_send.data, "%lf", num_double);
            break;
        default:
            //STRING case
            strcpy(packet_to_send.type, "STRING");
            strcpy(packet_to_send.data, packet.data);
            break;
    }
    return packet_to_send;
}

/**
 * Functie pentru verificarea existentei unui topic in lista de subscriptii a clientului.
*/
bool verify_subscribed_topic(struct client cl, char topic[51]) {
    for (auto it : cl.subscribed_topics) {
        if (strcmp(it.topic, topic) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Functie pentru gestionarea unui pachet primit de la un client UDP.
 * @param packet - pachetul primit de la clientul UDP
 * @param topic_list - lista de topicuri
 * @param registered_clients - map-ul cu clientii inregistrati
*/
void manage_packet_from_UDP (struct packet_to_TCP packet, vector<string> &topic_list,
                            unordered_map<string, struct client> &registered_clients) {

    if (find(topic_list.begin(), topic_list.end(), packet.topic) == topic_list.end()) {

        //Adaug noul topic in lista de topicuri.
        topic_list.push_back(packet.topic);

    } else {
        for (auto &it : registered_clients) {
            if (it.second.connected == true && verify_subscribed_topic(it.second, packet.topic) == true) {

                // Daca clientul este conectat si este abonat la topicul respectiv, trimit pachetul.
                int ret = send(it.second.sockfd, &packet, sizeof(struct packet_to_TCP), 0);
                if (ret < 0) {
                    fprintf(stderr, "Error sending packet to client.\n");
                }

            } else  if (it.second.connected == false && verify_subscribed_topic(it.second, packet.topic) == true) {

                // Daca clientul este deconectat si este abonat la topicul respectiv, adaug pachetul in coada de asteptare.
                bool exists = false;
                for (auto it2 : it.second.subscribed_topics) {
                    if (strcmp(it2.topic, packet.topic) == 0 && it2.SF == 1) {
                        exists = true;
                        break;
                    }
                }
                if (exists) {
                    it.second.waiting_topics.push_back(packet);
                }
            }
        }
    }
}

/**
 * Functie pentru gestionarea unui pachet primit de la un client TCP.
 * @param packet - pachetul primit de la clientul TCP
 * @param sockfd - socketul pe care se afla clientul TCP
 * @param pollfds - vectorul de file descriptors
 * @param registered_clients - map-ul cu clientii inregistrati
 * @param topics - lista de topicuri
*/
void manage_packet_from_TCP(struct packet_from_TCP packet, int sockfd,
                    vector<struct pollfd> &pollfds,
                    unordered_map<string, struct client>  &registered_clients,
                    vector<string> &topics) {

    if (find(topics.begin(), topics.end(), packet.topic) == topics.end()
        && strcmp(packet.topic, "exit") != 0) {

        // Topicul la care se doreste abonarea nu exista.
        struct packet_to_TCP packet_to_send;
        strcpy(packet_to_send.type, "error");
        strcpy(packet_to_send.data, "Topic does not exist.");
        int ret = send(sockfd, &packet_to_send, sizeof(struct packet_to_TCP), 0);
        if (ret < 0) {
            fprintf(stderr, "Error sending packet to client.\n");
        }
        return;
    }

    // Caut clientul in map-ul de clienti.
    struct client *client = nullptr;
    for (auto &it : registered_clients) {
        if (it.second.sockfd == sockfd) {
            client = &it.second;
            break;
        }
    }
    string topic = packet.topic;
    string action_type = packet.action_type;

    if (action_type == "sub") {
        struct packet_to_TCP packet_to_send;

        if (verify_subscribed_topic(*client, packet.topic)) {
            // Clientul deja a dat subscribe la acest topic.
            strcpy(packet_to_send.type, "error");
            strcpy(packet_to_send.data, "Client already subscribed to this topic.");
            int ret = send(sockfd, &packet_to_send, sizeof(struct packet_to_TCP), 0);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to client.\n");
            }
            return;
        }
        // Adaug topicul la care se doreste abonarea in lista de topicuri la care este abonat clientul.
        struct subscription sub;
        sub.SF = packet.SF;
        strcpy(sub.topic, packet.topic);
        client->subscribed_topics.push_back(sub);
        
        // Trimit mesajul de confirmare clientului.
        strcpy(packet_to_send.type, "success");
        strcpy(packet_to_send.data, "sub");
        int ret = send(sockfd, &packet_to_send, sizeof(struct packet_to_TCP), 0);
        if (ret < 0) {
                fprintf(stderr, "Error sending packet to client.\n");
            }
    } else if (action_type == "unsub") {
        struct packet_to_TCP packet_to_send;

        if (!verify_subscribed_topic(*client, packet.topic)) {
            // Clientul nu a dat subscribe la acel topic.
            strcpy(packet_to_send.type, "error");
            strcpy(packet_to_send.data, "Client didn't subscribe to this topic.");
            int ret = send(sockfd, &packet_to_send, sizeof(struct packet_to_TCP), 0);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to client.\n");
            }
            return;
        }

        // Sterg clientul din lista de topicuri.
        for (uint i = 0; i < client->subscribed_topics.size(); i++) {
            if (strcmp(client->subscribed_topics[i].topic, packet.topic) == 0) {
                client->subscribed_topics.erase(client->subscribed_topics.begin() + i);
                break;
            }
        }

        // Trimit mesajul de confirmare clientului.
        strcpy(packet_to_send.type, "success");
        strcpy(packet_to_send.data, "unsub");
        int ret = send(sockfd, &packet_to_send, sizeof(struct packet_to_TCP), 0);
        if (ret < 0) {
            fprintf(stderr, "Error sending packet to client.\n");
        }
    } else if (action_type == "exit") {
        // Inchid conexiunea clientului.
        close_client(sockfd, registered_clients);
        for (uint i = 0; i < pollfds.size(); i++) {
            if (pollfds[i].fd == sockfd) {
                pollfds.erase(pollfds.begin() + i);
                break;
            }
        }
        close(sockfd);
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int tcp_sockfd, udp_sockfd, port, newsockfd;
    struct sockaddr_in serv_addr;
    int ret;
    char buffer[BUFFLEN];

    if (argc == 0) {
        fprintf(stderr, "Usage : %s port\n", argv[0]);
        exit(0);
    }
    
    unordered_map<string, struct client> registered_clients;
    vector<string> topics;

    // Creez socketii TCP si UDP.
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd < 0) {
        fprintf(stderr, "Error opening TCP socket.\n");
        exit(0);
    }

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        fprintf(stderr, "Error opening UDP socket.\n");
        exit(0);
    }

    // Dezactivez algoritmul lui Nagle.
	int nagle = 1;
	ret = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
    if (ret < 0) {
        fprintf(stderr, "Error setting socket options.\n");
        exit(0);
    }

    int sock_opt = 1;
    ret = setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR,
                &sock_opt, sizeof(int));
    if (ret < 0) {
        fprintf(stderr, "Error setting socket options.\n");
        exit(0);
    }

    sock_opt = 1;
    ret = setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR,
                &sock_opt, sizeof(int));
    if (ret < 0) {
        fprintf(stderr, "Error setting socket options.\n");
        exit(0);
    }

    port = atoi(argv[1]);
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(tcp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    if (ret < 0) {
        fprintf(stderr, "Error binding TCP socket.\n");
        exit(0);
    }

    ret = bind(udp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    if (ret < 0) {
        fprintf(stderr, "Error binding UDP socket.\n");
        exit(0);
    }

    ret = listen(tcp_sockfd, MAX_CL);
    if (ret < 0) {
        fprintf(stderr, "Error listening.\n");
        exit(0);
    }
    // Adaug socketii in lista de descriptori pe care se va apela functia poll.
    vector<struct pollfd> pollfds;
    pollfds.push_back({STDIN_FILENO, POLLIN, 0});
    pollfds.push_back({udp_sockfd, POLLIN, 0});
    pollfds.push_back({tcp_sockfd, POLLIN, 0});

    while (1) {
        ret = poll(pollfds.data(), pollfds.size(), -1);
        if (ret < 0) {
            fprintf(stderr, "Error polling.\n");
            break;
        }

        if (pollfds[0].revents & POLLIN) {

            // Serverul primeste comanda de la tastatura.
            memset(buffer, 0, BUFFLEN);
            fgets(buffer, BUFFLEN, stdin);

            if (strncmp(buffer, "exit", 4) == 0) {
                close_connections(registered_clients);
                break;
            }
        }

        for (uint i = 1; i < pollfds.size(); i++) {
            if (pollfds[i].revents & POLLIN) {
                if (pollfds[i].fd == tcp_sockfd) {
                    sockaddr_in cli_addr;
                    socklen_t clilen = sizeof(cli_addr);
                    newsockfd = accept(tcp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
                    if (newsockfd < 0) {
                        fprintf(stderr, "Error accepting connection.\n");
                        continue;
                    }

                    // Am primit id-ul clientului.
                    memset(buffer, 0, BUFFLEN);
	                ret = recv(newsockfd, buffer, sizeof(buffer), 0);
                    if (ret < 0) {
                        fprintf(stderr, "Error receiving id.\n");
                        continue;
                    }
                    
                    // Verific statusul clientului.
                    ret = verify_client(newsockfd, buffer, registered_clients, cli_addr);
                    if (ret != -1) {
                        pollfds.push_back({newsockfd, POLLIN, 0});
                    }

                } else if (pollfds[i].fd == udp_sockfd) {
                    sockaddr_in cli_addr;
                    socklen_t clilen = sizeof(cli_addr);
                    struct packet_from_UDP packet;

                    ret = recvfrom(udp_sockfd, (char*)&packet, sizeof(struct packet_from_UDP), 0, (struct sockaddr *) &cli_addr, &clilen);
                    if (ret < 0) {
                        fprintf(stderr, "Error receiving packet from UDP client.\n");
                        continue;
                    }

                    if (ret == 0) {
                        continue; //clientul udp deconectat;
                    }

                    string ip = inet_ntoa(cli_addr.sin_addr);
                    int port = ntohs(cli_addr.sin_port);
                    struct packet_to_TCP packet_to_send = construct_TCP_packet(packet, ip, port);
                    manage_packet_from_UDP(packet_to_send, topics, registered_clients);
                } else {
                    // Am primit un mesaj de la un client TCP.
                    struct packet_from_TCP packet;
                    ret = recv(pollfds[i].fd, (char*)&packet, sizeof(struct packet_from_TCP), 0);
                    if (ret < 0) {
                        fprintf(stderr, "Error receiving packet from TCP client.\n");
                        continue;
                    }
                    manage_packet_from_TCP(packet, pollfds[i].fd, pollfds, registered_clients, topics);
                }
            }
        }
    }
    close(tcp_sockfd);
    close(udp_sockfd);
    return 0;
}