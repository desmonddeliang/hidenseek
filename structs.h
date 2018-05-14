#define MAX_PLAYER_NUM 5

struct hns_player
{
  uint32_t id;
  uint32_t role;
  float x;
  float y;
  uint32_t status;
  struct hns_player* next;
}__attribute__ ((packed)) ;
typedef struct hns_player hns_player_t;

struct hns_game {
  uint32_t timestamp;
  uint32_t num_players;
  uint32_t killer;
  uint32_t survivor;
  uint32_t points;
  uint32_t game_status;
  uint32_t num_freezed_players;
  hns_player_t obj1;
  hns_player_t obj2;
  uint32_t game_over;
  struct hns_player *players;
}__attribute__ ((packed)) ;
typedef struct hns_game hns_game_t;

struct hns_init
{
  uint32_t type;
  uint32_t role;
  uint32_t id;
} __attribute__ ((packed)) ;
typedef struct hns_init hns_init_t;


struct hns_init_ack
{
  uint32_t type;
  uint32_t ack_code;
} __attribute__ ((packed)) ;
typedef struct hns_init_ack hns_init_ack_t;


struct game_start
{
  uint32_t type;
  uint32_t num_players;
} __attribute__ ((packed)) ;
typedef struct game_start game_start_t;

struct hns_update
{
  uint32_t type;
  uint32_t id;
  float x;
  float y;
  uint32_t status;
} __attribute__ ((packed)) ;
typedef struct hns_update hns_update_t;


struct hns_broadcast_hdr
{
  uint32_t type;
  uint32_t num_players;
  uint32_t game_status;
  uint32_t points;
  uint64_t timestamp;
} __attribute__ ((packed)) ;
typedef struct hns_broadcast_hdr hns_broadcast_hdr_t;

struct hns_broadcast_player
{
  uint32_t role;
  float x;
  float y;
  uint32_t status;
}__attribute__ ((packed)) ;
typedef struct hns_broadcast_player hns_broadcast_player_t;
