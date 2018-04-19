/*
** Hide & Seek Game Server
*  CLient to server protocol
* -------------------------------------------------------------
* 4 Bytes | 4 Bytes | 4 Bytes | 4 Bytes
* -------------------------------------------------------------
* ID      | X       | Y       | facing
* -------------------------------------------------------------
*
*
* Server to client ai_protocol
* -------------------------------------------------------------
* 4 Bytes | 4 Bytes   | 4 Bytes   | 16 * n Bytes
* -------------------------------------------------------------
* Type    | Timestamp | n Entries | ID | X | Y | F | ID | X ...
* -------------------------------------------------------------
*/



#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "structs.h"

#define PORT "54321"   // port we're listening on
#define TICK_RATE 40000 // in microseconds

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void hns_update_status(hns_game_t *game, const void *buf){
  hns_player_t *new_input = (hns_player_t *)buf;
  hns_player_t *player_walker = game->players;

  if(player_walker==NULL){
    /* Add a new player record into our player table, assign new memory */
    hns_player_t *new_player = malloc(sizeof(hns_player_t));
    new_player->conn.id = new_input->conn.id;
    new_player->coor.x = new_input->coor.x;
    new_player->coor.y = new_input->coor.y;
    new_player->coor.f = new_input->coor.f;
    new_player->next = NULL;
    game->players = new_player;
    game->num_players++;

  }

  while(player_walker!=NULL){

    if(player_walker->conn.id==new_input->conn.id){
      /* Update this player */
      player_walker->coor.x = new_input->coor.x;
      player_walker->coor.y = new_input->coor.y;
      player_walker->coor.f = new_input->coor.f;

      break;
    }

    if(player_walker->next!=NULL){
      player_walker = player_walker->next;
    } else {
      /* Our table does NOT contain this current player */
      hns_player_t *new_player = malloc(sizeof(hns_player_t));
      new_player->conn.id = new_input->conn.id;
      new_player->coor.x = new_input->coor.x;
      new_player->coor.y = new_input->coor.y;
      new_player->coor.f = new_input->coor.f;
      new_player->next = NULL;
      player_walker->next = new_player;
      game->num_players++;

    }
  }

}


void hns_send_status(hns_game_t *game, uint8_t *send_buf){

  hns_update_header_t *update_hdr = malloc(sizeof(hns_update_header_t));

  update_hdr->type = 1;
  update_hdr->timestamp = game->timestamp;

  update_hdr->entry_count = game->num_players;

  memcpy(send_buf,update_hdr,sizeof(hns_update_header_t));

  /* copy all the players' status info */
  hns_player_t *player_walker = game->players;
  int i,j;

  for(i=0;i<game->num_players;i++){
    memcpy(send_buf + sizeof(hns_update_header_t) + i*(sizeof(hns_player_t)-8),
            player_walker, sizeof(hns_player_t)-8);
    player_walker = player_walker->next;
  }

  free(update_hdr);
}



int main(void)
{

  hns_game_t *game = malloc(sizeof(hns_game_t));
  game->timestamp = 0;
  game->num_players = 0;

  fd_set master;  // master file descriptor list
  fd_set read_fds;  // temp file descriptor list for select()
  int fdmax;    // maximum file descriptor number

  int listener;   // listening socket descriptor
  int newfd;    // newly accept()ed socket descriptor
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;

  char buf[256];  // buffer for client data
  int nbytes;

  char remoteIP[INET6_ADDRSTRLEN];

  int yes=1;    // for setsockopt() SO_REUSEADDR, below
  int i, j, rv;

  struct addrinfo hints, *ai, *p;

  FD_ZERO(&master);  // clear the master and temp sets
  FD_ZERO(&read_fds);

  // get us a socket and bind it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
    fprintf(stderr, "Hide & Seek Game Server: %s\n", gai_strerror(rv));
    exit(1);
  }

  for(p = ai; p != NULL; p = p->ai_next) {
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0) {
      continue;
    }

    // lose the pesky "address already in use" error message
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
      close(listener);
      continue;
    }

    break;
  }

  // if we got here, it means we didn't get bound
  if (p == NULL) {
    fprintf(stderr, "Hide & Seek Game Server: failed to bind\n");
    exit(2);
  }

  freeaddrinfo(ai); // all done with this

  // listen
  if (listen(listener, 10) == -1) {
    perror("listen");
    exit(3);
  }

  // add the listener to the master set
  FD_SET(listener, &master);

  // keep track of the biggest file descriptor
  fdmax = listener; // so far, it's this one

  uint8_t * send_buf = NULL;
  int send_len = 0;
  struct timeval * timeout = malloc(sizeof(struct timeval));
  timeout->tv_sec = 0;


  // main loop
  while(1) {
    game->timestamp = time(0);
    read_fds = master; // copy it
    timeout->tv_usec = TICK_RATE;
    if (select(fdmax+1, &read_fds, NULL, NULL, timeout) == -1) {
      perror("select");
      exit(4);
    } else {

    // run through the existing connections looking for data to read
    for(i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) { // we got one!!
        if (i == listener) {
          // handle new connections
          addrlen = sizeof remoteaddr;
          newfd = accept(listener,
            (struct sockaddr *)&remoteaddr,
            &addrlen);

          if (newfd == -1) {
            perror("accept");
          } else {
            FD_SET(newfd, &master); // add to master set
            if (newfd > fdmax) {  // keep track of the max
              fdmax = newfd;
            }
            printf("Hide & Seek Game Server: new connection from %s on "
              "socket %d\n",
              inet_ntop(remoteaddr.ss_family,
                get_in_addr((struct sockaddr*)&remoteaddr),
                remoteIP, INET6_ADDRSTRLEN),
              newfd);
          }
        } else {
          // handle data from a client
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
            // got error or connection closed by client
            if (nbytes == 0) {
              // connection closed
              printf("Hide & Seek Game Server: socket %d hung up\n", i);
              return 0;
            } else {
              perror("recv");
              return 0;
            }
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          } else {
            hns_update_status(game, buf);
          }
        } // END handle data from client
      } // END got new incoming connection
    } // END looping through file descriptors

    /* Broadcast player info */
    if(game->num_players>0){
      //printf("Broadcasting: %d\n", game->timestamp);
      // Player Status Broadcasting for every 100 miliseconds
      send_len = sizeof(hns_update_header_t) +
                          game->num_players * (sizeof(hns_player_t)-8);
      send_buf = malloc(send_len);
      hns_send_status(game, send_buf);

      for(j = 0; j <= fdmax; j++) {
          // send to everyone!
          if (FD_ISSET(j, &master)) {
              // except the listener
              if (j != listener) {
                  if (send(j, send_buf, send_len, 0) == -1) {
                      perror("send");
                  }
              }
          }
      }
      free(send_buf);
    }
  }
  } // END while


  return 0;
}
