#pragma once

#include "raft/raft.h"

#define RPS_DEFAULT_DATA_DIR "var/rps"
#define RPS_DEFAULT_HOST "127.0.0.1"

typedef struct {
    char node_id[RAFT_MAX_ID];
    char player_id[32];
    char host[256];
    int port;
    int port_supplied;
    char data_dir[256];
    char initial_members[RAFT_MAX_NODES][RAFT_MAX_ID];
    size_t member_count;
    char peer_ids[RAFT_MAX_PEERS][RAFT_MAX_ID];
    char peer_hosts[RAFT_MAX_PEERS][256];
    int peer_ports[RAFT_MAX_PEERS];
    size_t peer_count;
    char join_host[256];
    int join_port;
} rps_args_t;

rps_args_t rps_args_parse(int argc, char **argv);
