#define MAX_PLAYER_NUM 5

struct hns_game {
  uint32_t timestamp;
  int num_players;
  int killer;
  int survivor;
  struct hns_player *players;
}__attribute__ ((packed)) ;
typedef struct hns_game hns_game_t;


struct hns_player
{
  int id;
  int role;
  struct hns_player* next;
}__attribute__ ((packed)) ;
typedef struct hns_player hns_player_t;


struct hns_init
{
  int type;
  int role;
  uint32_t id;
} __attribute__ ((packed)) ;
typedef struct hns_init hns_init_t;


struct hns_init_ack
{
  int type;
  int ack_code;
} __attribute__ ((packed)) ;
typedef struct hns_init_ack hns_init_ack_t;


struct game_start
{
  int type;
  int num_players;
  hns_player_t* players;
} __attribute__ ((packed)) ;
typedef struct game_start game_start_t;
