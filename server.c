#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>

#include "structs.h"

#define PORT "54321"

void game_init(hns_game_t*);
void* get_in_addr(struct sockaddr*);
void hns_handle_init(hns_game_t* game, char* buf, int i/* socket descriptor*/ );

int main(void)
{
  hns_game_t* game = malloc(sizeof(hns_game_t));
  game_init(game);

  fd_set master;    // master file descriptor list
  fd_set read_fds;  // temp file descriptor list for select()
  int fdmax;        // maximum file descriptor number

  int listener;     // listening socket descriptor
  int newfd;        // newly accept()ed socket descriptor
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;

  char buf[1024];    // buffer for client data
  int nbytes;

  char remoteIP[INET6_ADDRSTRLEN];

  int yes = 1;        // for setsockopt() SO_REUSEADDR, below
  int i, j, rv;

  struct addrinfo hints, *ai, *p;

  FD_ZERO(&master);    // clear the master and temp sets
  FD_ZERO(&read_fds);

  // get us a socket and bind it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0){
    fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
    exit(1);
  }

  for(p = ai; p != NULL; p = p->ai_next){
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0)
      continue;
    // lose the pesky "address already in use" error message
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0){
      close(listener);
      continue;
    }
    break;
  }

  // if we got here, it means we didn't get bound
  if (p == NULL){
    fprintf(stderr, "selectserver: failed to bind\n");
    exit(2);
  }
  freeaddrinfo(ai); // all done with this
  // listen
  if (listen(listener, 10) == -1)
  {
    perror("listen");
    exit(3);
  }
  // add the listener to the master set
  FD_SET(listener, &master);
  // keep track of the biggest file descriptor
  fdmax = listener; // so far, it's this one

  // main loop
  while(game->num_players < 2)
  {
    read_fds = master; // copy it
    if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
    {
      perror("select");
      exit(4);
    }
    // run through the existing connections looking for data to read
    for(i = 0; i <= fdmax; i++)
    {
      if(FD_ISSET(i, &read_fds))
      { // we got one!!
        if (i == listener)
        {
          // handle new connections
          addrlen = sizeof remoteaddr;
          newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

          if (newfd == -1)
          {
            perror("accept");
          }
          else
          {
            FD_SET(newfd, &master); // add to master set
            if (newfd > fdmax)
            {    // keep track of the max
              fdmax = newfd;
            }
            printf("selectserver: new connection from %s on socket %d\n",
              inet_ntop(remoteaddr.ss_family,
              get_in_addr((struct sockaddr*)&remoteaddr),
              remoteIP, INET6_ADDRSTRLEN),
              newfd);
          }
        }
        else
        {
          // handle data from a client
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
          {
            // got error or connection closed by client
            if (nbytes == 0)
            {
              // connection closed
              printf("selectserver: socket %d hung up\n", i);
            }
            else
              perror("recv");
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          }
          else
          {
            /* initial information from client */
            hns_handle_init(game, buf, i);
          }
        } // END handle data from client
      } // END got new incoming connection
    } // END looping through file descriptors
  } // END main while loop



  /* Enters Gaming stage by sending a game start signal */
  game_start_t* gamestart = malloc(sizeof(game_start_t));
  gamestart->type = htons(2);
  gamestart->num_players = htons(game->num_players);
  gamestart->players = game->players;

  /* broadcast to all players game start */
  for(j = 0; j <= fdmax; j++)
    if (FD_ISSET(j, &master))
      if (j != listener)
        while(send(j, gamestart, sizeof(game_start_t), 0) == -1)
          perror("game start");

  free(gamestart);
  return 0;
}


void hns_handle_init(hns_game_t* game, char* buf, int i/* socket descriptor*/ ){

  hns_init_t* init = (hns_init_t*)buf;

  printf("type: %d\n", ntohs(init->type));
  printf("role: %d\n", ntohs(init->role));
  printf("id: %d\n", ntohs(init->id));

  int ack_code = 0;  // 0: Confirmed Killer, 1: Confirmed Survivor, 2: Invalid

  if(ntohs(init->role) == 0){
    /* I want to be a killer */
    if(game->killer == 0){
      /* Ok you can be a killer */
      game->killer = 1;
      ack_code = 0;
    } else {
      /* Oops, killer already filled */
      game->survivor += 1;
      ack_code = 1;
    }
  } else {
    /* Ok you can be a survivor */
    init->role = htons(1);
    game->survivor += 1;
    ack_code = 1;
  }

  /* Check for duplicate ID */
  hns_player_t* player_walker = game->players;
  while(player_walker) {
    if(player_walker->id == ntohs(init->id)) ack_code = 2;
    player_walker = player_walker->next;
  }

  /* Send out init ACK */
  hns_init_ack_t* init_ack = malloc(sizeof(hns_init_ack_t));
  init_ack->type = htons(1);
  init_ack->ack_code = htons(ack_code);
  if(send(i, init_ack, sizeof(hns_init_ack_t), 0) == -1)
    perror("Init_ack");
  free(init_ack);

  /* add player into playerlist */
  if(ack_code != 2){
    hns_player_t* player = malloc(sizeof(hns_player_t));
    player->id = ntohs(init->id);
    player->role = ntohs(init->role);

    game->num_players += 1;
    player_walker = game->players;

    while(player_walker != NULL & player_walker->next != NULL)
      player_walker = player_walker->next;

    player_walker->next = player;
  }
}


//==============================================================================
// initialization
//==============================================================================
void game_init(hns_game_t* game)
{
  game->players = malloc(MAX_PLAYER_NUM * sizeof(hns_player_t));
  game->timestamp = time(0);
  game->num_players = 0;
  game->killer = 0;
  game->survivor = 0;
}

//==============================================================================
// get_in_addr
//==============================================================================
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
