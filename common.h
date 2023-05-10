#ifndef _COMMON_H
#define _COMMON_H 1

#include <iostream>
#include <vector>
#include <map>
#include <bits/stdc++.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <limits.h>
#include <sys/poll.h>

using namespace std;

#define SIZE 1000
#define BUFFLEN 256
#define MAX_CL 30

struct packet_from_TCP {
    char topic[50];
    int SF;
    char action_type[6];
};

struct packet_from_UDP {
    char topic[50];
    uint8_t type;
    char data[1500];
}__attribute__((packed));

struct packet_to_TCP {
    char ip[16];
    int port;
    char topic[51];
    char type[11];
    char data[1500];
}__attribute__((packed));

struct subscription {
    int SF;
    char topic[51];
}__attribute__((packed));

struct client {
    string id;
    int sockfd;
    int port;
    vector<packet_to_TCP> waiting_topics;
    vector<subscription> subscribed_topics;
    string ip;
    bool connected;
};

#endif