#pragma once

#include "rxnet/fsm.h"

#include <stddef.h>

#define RPS_MAX_PLAYERS 8
#define RPS_MAX_ID 32
#define RPS_HASH_HEX 65
#define RPS_MAX_HISTORY 32
#define RPS_MAX_TEXT 128

typedef enum {
    RPS_STATE_WAITING_PLAYERS = 0,
    RPS_STATE_CONFIGURING_GAME = 1,
    RPS_STATE_ROUND_OPEN = 2,
    RPS_STATE_WAITING_COMMITS = 3,
    RPS_STATE_WAITING_REVEALS = 4,
    RPS_STATE_RESOLVING_ROUND = 5,
    RPS_STATE_ROUND_FINISHED = 6,
    RPS_STATE_GAME_OVER = 7
} rps_state_t;

typedef enum {
    RPS_MOVE_ROCK = 0,
    RPS_MOVE_PAPER = 1,
    RPS_MOVE_SCISSORS = 2
} rps_move_t;

typedef struct {
    int target_score;
    int max_rounds;
    int commit_timeout_ticks; /* -1 = infinite */
    int reveal_timeout_ticks; /* -1 = infinite */
} rps_game_config_t;

typedef struct {
    char player_id[RPS_MAX_ID];
    char hash[RPS_HASH_HEX];
} rps_commit_t;

typedef struct {
    char player_id[RPS_MAX_ID];
    rps_move_t move;
} rps_reveal_t;

typedef struct {
    char player_id[RPS_MAX_ID];
    char commit_hash[RPS_HASH_HEX];
    rps_move_t reveal_move;
    char reveal_nonce[RPS_MAX_TEXT];
    int has_joined_player;
    int confirm_players;
    int config_committed;
    int round_started;
    int has_commit;
    int has_reveal;
} rps_game_inputs_t;

typedef struct {
    rx_fsm_runtime runtime;
    rx_fsm_machine machine;
    rps_game_inputs_t inputs;
    rps_game_inputs_t latched;
    rps_game_config_t config;
    char players[RPS_MAX_PLAYERS][RPS_MAX_ID];
    int scores[RPS_MAX_PLAYERS];
    size_t player_count;
    rps_commit_t commits[RPS_MAX_PLAYERS];
    size_t commit_count;
    rps_reveal_t reveals[RPS_MAX_PLAYERS];
    size_t reveal_count;
    int round_number;
    int tick;
    int deadline_tick;
    char winner[RPS_MAX_ID];
    char last_announcement[RPS_MAX_TEXT];
    char last_error[RPS_MAX_TEXT];
} rps_game_t;

rps_game_config_t rps_game_default_config(void);
void rps_game_init(rps_game_t *game);
rps_state_t rps_game_state(const rps_game_t *game);
const char *rps_state_name(rps_state_t state);
const char *rps_move_name(rps_move_t move);
int rps_parse_move(const char *text, rps_move_t *move);
int rps_make_commitment(const char *player_id, rps_move_t move, const char *nonce,
                        char *out_hex, size_t out_size);

int rps_game_add_player(rps_game_t *game, const char *player_id);
int rps_game_confirm_players(rps_game_t *game);
int rps_game_configure(rps_game_t *game, const rps_game_config_t *config);
int rps_game_start_round(rps_game_t *game);
int rps_game_submit_commit(rps_game_t *game, const char *player_id, const char *commit_hash);
int rps_game_reveal(rps_game_t *game, const char *player_id, rps_move_t move, const char *nonce);
void rps_game_tick(rps_game_t *game, int count);
int rps_game_score(const rps_game_t *game, const char *player_id);
