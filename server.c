#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include "structs.h"

#define PORT "54321"

void game_init(hns_game_t*);
void* get_in_addr(struct sockaddr*);
void hns_handle_init(hns_game_t* game, char* buf, int i/* socket descriptor*/ );
void gaming(hns_game_t* game);

void hns_obj(hns_game_t* game, hns_player_t* player);
void hns_judge(hns_game_t* game);
double hns_distance(hns_player_t* p1, hns_player_t* p2);



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
  while(game->num_players < 3)
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


  printf("GAMING TIME!\n");
  /* Enters Gaming stage by sending a game start signal */
  game_start_t* gamestart = malloc(sizeof(game_start_t));
  gamestart->type = 2;
  gamestart->num_players = game->num_players;

  /* broadcast to all players the game start signal */
  for(j = 0; j <= fdmax; j++)
    if (FD_ISSET(j, &master))
      if (j != listener)
        while(send(j, gamestart, sizeof(game_start_t), 0) == -1)
          perror("game start");

  free(gamestart);

  /* start broadcasting every player's information to everybody */
  gaming(game);

  return 0;
}


void hns_handle_init(hns_game_t* game, char* buf, int i/* socket descriptor*/ ){

  hns_init_t* init = (hns_init_t*)buf;

  printf("type: %u\n", init->type);
  printf("role: %u\n", init->role);
  printf("id: %u\n", init->id);

  uint32_t ack_code = 0;  // 0: Confirmed Killer, 1~4 for survivor

  if(init->role == 0){
    /* I want to be a killer */
    if(game->killer == 0){
      /* Ok you can be a killer */
      game->killer = 1;
      ack_code = 0;
    } else {
      /* Oops, killer already filled */
      game->survivor += 1;
      ack_code = game->survivor;
      init->role = game->survivor;
    }
  } else {
    /* Ok you can be a survivor */
    game->survivor += 1;
    ack_code = game->survivor;
    init->role = game->survivor;
  }

  /* Check for duplicate ID */
  hns_player_t* player_walker = game->players;
  while(player_walker!=NULL) {
    if(player_walker->id == ntohs(init->id)) ack_code = 5;
    player_walker = player_walker->next;
  }

  /* Send out init ACK */
  hns_init_ack_t* init_ack = malloc(sizeof(hns_init_ack_t));
  init_ack->type = 1;
  init_ack->ack_code = ack_code;
  if(send(i, init_ack, sizeof(hns_init_ack_t), 0) == -1)
    perror("Init_ack");
  printf("Sending init Ack with code %i\n", init_ack->ack_code);
  free(init_ack);


  /* add player into playerlist */
  if(ack_code != 5){
    hns_player_t* player = malloc(sizeof(hns_player_t));
    player->id = init->id;
    player->role = init->role;
    player->next = NULL;

    /* init player position */
    if(ack_code == 0){
      player->x = 0;
      player->y = 0;
    }
    if(ack_code == 1){
      player->x = 3;
      player->y = 3;
    }
    if(ack_code == 2){
      player->x = 3;
      player->y = -3;
    }
    if(ack_code == 3){
      player->x = -3;
      player->y = 3;
    }
    if(ack_code == 4){
      player->x = -3;
      player->y = -3;
    }

    printf("Player Connected with role %i\n",init->role );

    game->num_players += 1;
    player_walker = game->players;

    if(player_walker==NULL){
      /* empty player list */
      game->players = player;
    } else {
      /* traverse player list to add player */
      while(player_walker->next != NULL){
        player_walker = player_walker->next;
      }
      player_walker->next = player;
    }
  }
}


//==============================================================================
// initialization
//==============================================================================
void game_init(hns_game_t* game)
{
  game->players = NULL;
  game->timestamp = time(0);
  game->num_players = 0;
  game->killer = 0;
  game->survivor = 0;
  game->game_status = 0;
  game->points = 0;
  game->obj1.x = -3;
  game->obj2.x = 3;
  game->obj1.y = 0;
  game->obj2.y = 0;
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


//==============================================================================
// gaming
//==============================================================================
void gaming(hns_game_t* game)
{
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  struct sockaddr_storage their_addr;
  char buf[1024];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, "54322", &hints, &servinfo);

  for(p = servinfo; p != NULL; p = p->ai_next)
  {
    if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("listener: socket");
      continue;
    }

    if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      perror("listener: bind");
      continue;
    }
    break;
  }

  if(p == NULL)
  {
    perror("listener: failed to bind socket");
    return;
  }

  freeaddrinfo(servinfo);

  struct timeval tv;
  fd_set read_fds;

  struct timeval current_time, last_sent;
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  struct sockaddr_in addr_list[MAX_PLAYER_NUM];
  memset(addr_list, 0 , sizeof(struct sockaddr_in)*MAX_PLAYER_NUM);

  gettimeofday(&last_sent, NULL);
  while(1)
  {
    memset(buf, 0, strlen(buf));
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    select(sockfd + 1, &read_fds, NULL, NULL, NULL);
    if(FD_ISSET(sockfd, &read_fds))
    {
      numbytes = recvfrom(sockfd, buf, sizeof(hns_update_t), 0, (struct sockaddr*)&cli_addr, &clilen);

      int add = 1;
      for(int i = 0; i < MAX_PLAYER_NUM; i++)
      {
        if(addr_list[i].sin_port == cli_addr.sin_port && addr_list[i].sin_addr.s_addr == cli_addr.sin_addr.s_addr)
         add = 0;
      }

      if(add)
      {
        for(int i = 0; i < MAX_PLAYER_NUM; i++)
        {
          if(addr_list[i].sin_port == 0)
          {
            addr_list[i] = cli_addr;
            break;
          } // END if NULL
        } // END for add
      } // END if add

      hns_update_t* update = (hns_update_t*)buf;
      if(update->type == 3)
      {
        hns_player_t* player_walker = game->players;
        while(player_walker)
        {
          if(player_walker->id == update->id)
          {
            player_walker->x = update->x;
            player_walker->y = update->y;
            hns_obj(game,player_walker);
            //player_walker->status = update->status;
          } // end same id
          player_walker = player_walker->next;
        } // end find id
      } // end if update
    }

    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec * 1000000 + current_time.tv_usec - (last_sent.tv_sec * 1000000 + last_sent.tv_usec) > 100000)
    {

      hns_judge(game);

      hns_broadcast_hdr_t* bro = malloc(sizeof(hns_broadcast_hdr_t));
      bro->type = 4;
      bro->num_players = game->num_players;
      bro->game_status = game->game_status;
      bro->points = game->points;
      bro->timestamp = current_time.tv_sec * 1000000 + current_time.tv_usec;

      int send_buf_len = sizeof(hns_broadcast_hdr_t) + sizeof(hns_broadcast_player_t) * game->num_players;
      uint8_t* send_buf = malloc(send_buf_len);
      hns_broadcast_player_t* player_temp = malloc(sizeof(hns_broadcast_player_t));
      memset(player_temp, 0 ,sizeof(hns_broadcast_player_t));

      memcpy(send_buf, bro, sizeof(hns_broadcast_hdr_t));

      hns_player_t* player_walker = game->players;

      int player_index = 0;

      while(player_walker!=NULL){
        player_temp->role = player_walker->role;
        player_temp->x = player_walker->x;
        player_temp->y = player_walker->y;
        player_temp->status = player_walker->status;
        player_walker = player_walker->next;
        memcpy(send_buf + sizeof(hns_broadcast_hdr_t) + player_index*sizeof(hns_broadcast_player_t),
                player_temp, sizeof(hns_broadcast_player_t));
        player_index++;
      }

      for(int i = 0; i < game->num_players; i++)
      {
        if(addr_list[i].sin_port!=0){ /* This client is set */
          sendto(sockfd, send_buf, send_buf_len, 0, (struct sockaddr*) &addr_list[i], sizeof(cli_addr));
        }
      } // END for add
      free(bro);
      free(send_buf);



      gettimeofday(&last_sent, NULL);
    }
  }

}

void hns_obj(hns_game_t* game, hns_player_t* player){
  /* assume the first player is killer */
  if(player->role!=0){
    if(hns_distance(&(game->obj1), player)<0.5 || hns_distance(&(game->obj2), player)<0.5){
      /* Add points */
      game->points++;
    }
  } else {
    if(hns_distance(&(game->obj1), player)<0.5 || hns_distance(&(game->obj2), player)<0.5){
      /* Add points */
      if(game->points>0){
          game->points--;
      }
    }
  }
}


void hns_judge(hns_game_t* game){
  /* assume the first player is killer */
  hns_player_t* killer = game->players;
  hns_player_t* player_walker = killer->next;
  hns_player_t* player_walker2 = killer->next;

  while(player_walker){
    if(hns_distance(killer, player_walker)<0.2){
      /* Killer Got a survivor */
      if(player_walker->status%2==0){
        player_walker->status++;
        printf("Survivor %i Freezed, status: %i.\n", player_walker->role,player_walker->status);
      }
    }

    player_walker2 = killer->next;
    if(player_walker->status%2==1 & player_walker->status!=5){
      /* Survivor is frozen, any help? */
      while(player_walker2!=NULL){
        if(player_walker2!=player_walker){
          if(hns_distance(player_walker, player_walker2)<0.2){
            /* Survivor got freed by another player */
            player_walker->status++;
            printf("Survivor Freed.\n");
          }
        }
        player_walker2 = player_walker2->next;
      }
    }

    player_walker = player_walker->next;
  }

}




double hns_distance(hns_player_t* p1, hns_player_t* p2){
  return sqrt((double)((p1->x-p2->x)*(p1->x-p2->x)+(p1->y-p2->y)*(p1->y-p2->y)));
}
