#pragma once

#include "raft/raft.h"
#include "rps/rps_command.h"

typedef void (*rps_peer_set_fn)(void *user, const char *node_id, const char *address);

typedef struct {
    rps_game_t game;
    char applied_keys[RAFT_MAX_LOG][RAFT_MAX_KEY];
    size_t applied_key_count;
    char applied_operation_ids[RAFT_MAX_LOG][RAFT_MAX_KEY];
    size_t applied_operation_id_count;
    raft_command_t applied_commands[RAFT_MAX_LOG];
    size_t applied_command_count;
    rps_peer_set_fn peer_set;
    void *peer_set_user;
} rps_app_t;

raft_application_t rps_app_make(rps_app_t *state);
raft_application_t rps_app_make_with_peers(rps_app_t *state, rps_peer_set_fn peer_set, void *peer_set_user);
