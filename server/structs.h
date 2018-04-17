#define MAX_PLAYER_NUM 5

struct hns_player_coordinates {
  uint32_t x;
  uint32_t y;
  uint32_t f; /* currently facing which direction, measured in degrees */
}__attribute__ ((packed)) ;
typedef struct hns_player_coordinates hns_player_coordinates_t;


struct hns_player_connection {
  uint32_t id;
}__attribute__ ((packed)) ;
typedef struct hns_player_connection hns_player_connection_t;


struct hns_player_status {
  uint32_t role; /* 0 for survivor, 1 for killer */
  uint32_t hp; /* health poitns, 100 max */
  uint32_t speed;
}__attribute__ ((packed)) ;
typedef struct hns_player_status hns_player_status_t;


struct hns_update_header {
  uint32_t type;
  uint32_t timestamp;
  uint32_t entry_count;
}__attribute__ ((packed)) ;
typedef struct hns_update_header hns_update_header_t;


struct hns_player {
  struct hns_player_connection conn;
  struct hns_player_coordinates coor;
  struct hns_player *next;
}__attribute__ ((packed)) ;
typedef struct hns_player hns_player_t;


struct hns_game {
  uint32_t timestamp;
  uint32_t num_players;
  struct hns_player *players;
}__attribute__ ((packed)) ;
typedef struct hns_game hns_game_t;


struct hns_demo {
  uint32_t id;
  uint32_t x;
  uint32_t y;
} __attribute__ ((packed)) ;
typedef struct hns_demo hns_demo_t;
