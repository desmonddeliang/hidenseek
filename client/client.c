/*
** client.c -- a stream socket client demo
*/


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../server/structs.h"
#include "conio.h"

#include <arpa/inet.h>

#define PORT "54321" // the port client will be connecting to

#define MAXDATASIZE 1024 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv, i;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    srand(time(NULL));

    hns_player_t *player = malloc(sizeof(hns_player_t));

    player->conn.id = rand();
    player->coor.x = 244;
    player->coor.y = 566;
    player->coor.f = 90;

    send(sockfd, player, sizeof(hns_player_t),0);


    hns_player_t *players = NULL;
    hns_update_header_t *update_hdr = NULL;

    FILE *fp;
    char ch;

    while(1){
      if (kbhit){
        // Stores the pressed key in ch
        ch = getch();

        // Terminates the loop
        // when escape is pressed
        if (int(ch) == 27)
          break;

        cout << "\nKey pressed= " << ch;
      } else {
        if(recv(sockfd, buf, MAXDATASIZE, 0)>0){
          fp = fopen("players.txt","w");

          update_hdr = (hns_update_header_t *)buf;
          fprintf(fp, "%i\n", update_hdr->entry_count);
          for(i=0;i<update_hdr->entry_count;i++){
            players = (hns_player_t *)(buf +
                                      sizeof(hns_update_header_t) +
                                      i * sizeof(hns_player_t));
            fprintf(fp, "%i\n", players->conn.id);
            fprintf(fp, "%i\n", players->coor.x);
            fprintf(fp, "%i\n", players->coor.y);
            fprintf(fp, "%i\n", players->coor.f);
          }
          fclose(fp);
        }
      }
    }
    close(sockfd);
    return 0;
}
