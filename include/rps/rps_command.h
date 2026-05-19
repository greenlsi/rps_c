#pragma once

#include "raft/raft.h"
#include "rps/rps_game.h"

typedef enum {
    RPS_OP_UNKNOWN = 0,
    RPS_OP_PLAYER_JOINED,
    RPS_OP_CONFIRM_PLAYERS,
    RPS_OP_CONFIGURE,
    RPS_OP_START_ROUND,
    RPS_OP_COMMIT_MOVE,
    RPS_OP_REVEAL_MOVE,
    RPS_OP_PEER_SET
} rps_operation_kind_t;

typedef struct {
    rps_operation_kind_t kind;
    char operation_id[RAFT_MAX_KEY];
    char player_id[RPS_MAX_ID];
    rps_game_config_t config;
    char commit_hash[RPS_HASH_HEX];
    rps_move_t move;
    char nonce[RPS_MAX_TEXT];
    char peer_value[RAFT_MAX_VALUE];
} rps_operation_t;

int rps_command_encode(const rps_operation_t *operation, int sequence, raft_command_t *command);
int rps_command_decode(const raft_command_t *command, rps_operation_t *operation);
const char *rps_operation_name(rps_operation_kind_t kind);
