#include "rps/rps_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *rps_operation_name(rps_operation_kind_t kind) {
    switch (kind) {
        case RPS_OP_PLAYER_JOINED: return "player_joined";
        case RPS_OP_CONFIRM_PLAYERS: return "confirm_players";
        case RPS_OP_CONFIGURE: return "configure";
        case RPS_OP_START_ROUND: return "start_round";
        case RPS_OP_COMMIT_MOVE: return "commit_move";
        case RPS_OP_REVEAL_MOVE: return "reveal_move";
        case RPS_OP_PEER_SET: return "peer_set";
        default: return "unknown";
    }
}

static rps_operation_kind_t kind_from_name(const char *name) {
    if (strcmp(name, "player_joined") == 0) return RPS_OP_PLAYER_JOINED;
    if (strcmp(name, "confirm_players") == 0) return RPS_OP_CONFIRM_PLAYERS;
    if (strcmp(name, "configure") == 0) return RPS_OP_CONFIGURE;
    if (strcmp(name, "start_round") == 0) return RPS_OP_START_ROUND;
    if (strcmp(name, "commit_move") == 0) return RPS_OP_COMMIT_MOVE;
    if (strcmp(name, "reveal_move") == 0) return RPS_OP_REVEAL_MOVE;
    if (strcmp(name, "peer_set") == 0) return RPS_OP_PEER_SET;
    return RPS_OP_UNKNOWN;
}

int rps_command_encode(const rps_operation_t *operation, int sequence, raft_command_t *command) {
    const char *name = rps_operation_name(operation->kind);
    memset(command, 0, sizeof(*command));
    snprintf(command->op, sizeof(command->op), "rps.%s", name);
    if (operation->operation_id[0])
        snprintf(command->key, sizeof(command->key), "%s", operation->operation_id);
    else
        snprintf(command->key, sizeof(command->key), "%08d", sequence);
    switch (operation->kind) {
        case RPS_OP_PLAYER_JOINED:
            snprintf(command->value, sizeof(command->value), "%s", operation->player_id);
            break;
        case RPS_OP_CONFIRM_PLAYERS:
        case RPS_OP_START_ROUND:
            command->value[0] = '\0';
            break;
        case RPS_OP_CONFIGURE:
            snprintf(command->value, sizeof(command->value), "%d,%d,%d,%d",
                     operation->config.target_score, operation->config.max_rounds,
                     operation->config.commit_timeout_ticks, operation->config.reveal_timeout_ticks);
            break;
        case RPS_OP_COMMIT_MOVE:
            snprintf(command->value, sizeof(command->value), "%s,%s",
                     operation->player_id, operation->commit_hash);
            break;
        case RPS_OP_REVEAL_MOVE:
            snprintf(command->value, sizeof(command->value), "%s,%d,%s",
                     operation->player_id, (int)operation->move, operation->nonce);
            break;
        case RPS_OP_PEER_SET:
            snprintf(command->value, sizeof(command->value), "%s", operation->peer_value);
            break;
        default:
            return -1;
    }
    return 0;
}

int rps_command_decode(const raft_command_t *command, rps_operation_t *operation) {
    char value[RAFT_MAX_VALUE];
    char *a, *b, *c, *save = NULL;
    memset(operation, 0, sizeof(*operation));
    if (strncmp(command->op, "rps.", 4) != 0) return -1;
    operation->kind = kind_from_name(command->op + 4);
    strncpy(operation->operation_id, command->key, sizeof(operation->operation_id) - 1);
    strncpy(value, command->value, sizeof(value) - 1);
    switch (operation->kind) {
        case RPS_OP_PLAYER_JOINED:
            strncpy(operation->player_id, value, sizeof(operation->player_id) - 1);
            return 0;
        case RPS_OP_CONFIRM_PLAYERS:
        case RPS_OP_START_ROUND:
            return 0;
        case RPS_OP_CONFIGURE:
            a = strtok_r(value, ",", &save);
            b = strtok_r(NULL, ",", &save);
            c = strtok_r(NULL, ",", &save);
            if (!a || !b || !c) return -1;
            operation->config.target_score = atoi(a);
            operation->config.max_rounds = atoi(b);
            operation->config.commit_timeout_ticks = atoi(c);
            a = strtok_r(NULL, ",", &save);
            if (!a) return -1;
            operation->config.reveal_timeout_ticks = atoi(a);
            return 0;
        case RPS_OP_COMMIT_MOVE:
            a = strtok_r(value, ",", &save);
            b = strtok_r(NULL, ",", &save);
            if (!a || !b) return -1;
            strncpy(operation->player_id, a, sizeof(operation->player_id) - 1);
            strncpy(operation->commit_hash, b, sizeof(operation->commit_hash) - 1);
            return 0;
        case RPS_OP_REVEAL_MOVE:
            a = strtok_r(value, ",", &save);
            b = strtok_r(NULL, ",", &save);
            c = strtok_r(NULL, ",", &save);
            if (!a || !b || !c) return -1;
            strncpy(operation->player_id, a, sizeof(operation->player_id) - 1);
            operation->move = (rps_move_t)atoi(b);
            strncpy(operation->nonce, c, sizeof(operation->nonce) - 1);
            return 0;
        case RPS_OP_PEER_SET:
            strncpy(operation->peer_value, value, sizeof(operation->peer_value) - 1);
            return 0;
        default:
            return -1;
    }
}
