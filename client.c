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

#define PORT  "54321"
#define BUFFER_SIZE 1024
char* SERVER_IP = "127.0.0.1";

int main()
{
  int err;
  int sockfd;
  struct addrinfo hints, *res;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  getaddrinfo(SERVER_IP, PORT, &hints, &res);

  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  err = connect(sockfd, res->ai_addr, res->ai_addrlen);
  if(err < 0)
  {
    perror("Connection error");
    exit(1);
  }
  printf("Connected!\n");

  int role;
  int id;
  printf("The role you want to play: ");
  scanf("%d", &role);
  printf("Your id: ");
  scanf("%i", &id);

  hns_init_t* init = malloc(sizeof(hns_init_t));
  init->type = htons(0);
  init->role = htons(role);
  init->id = htons(id);

  send(sockfd, init, sizeof(hns_init_t), 0);

  char* buf[BUFFER_SIZE];
  recv(sockfd, buf, 1024, 0);
  hns_init_ack_t* init_ack = (hns_init_ack_t*)buf;

  printf("type: %d\n", ntohs(init_ack->type));
  printf("ack_code: %d\n", ntohs(init_ack->ack_code));


  memset(buf, 0, 1024);
  recv(sockfd, buf, 1024, 0);
  game_start_t* start = (game_start_t*)buf;

  printf("type: %d\n", ntohs(start->type));
  printf("Num: %d\n", ntohs(start->num_players));
  int num_players = ntohs(start->num_players);

  hns_player_t* playerlist = malloc(sizeof(hns_player_t) * num_players);
  printf("HER\n");
  hns_player_t* temp = start->players;
  hns_player_t* ptr = playerlist;
  while(temp)
  {
    memcpy(ptr, temp, sizeof(hns_player_t));
    temp = temp->next;
    ptr  = ptr->next;
  }

  close(sockfd);

  return 0;
}
