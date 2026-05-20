#pragma once

#include "raft/raft.h"
#include "raft/raft_tcp_transport.h"
#include "rxnet/coop.h"
#include "rxnet/fsm.h"
#include "rps/rps_app.h"

#define RPS_CLI_READY 0

typedef struct {
    char node_id[RAFT_MAX_ID];
    char player_id[RPS_MAX_ID];
    char line[512];
    int has_line;
    int prompt_needed;
    int quit_requested;
    int last_role;
    int last_term;
    int last_commit_index;
    int sequence;
    raft_tcp_join_req_t pending_joins[RAFT_MAX_NODES];
    size_t pending_join_count;
    raft_command_t fwd_cmds[RAFT_MAX_QUEUE];
    size_t fwd_cmd_count;
    raft_node_t *node;
    raft_tcp_transport_t *tcp;
    rps_app_t *app;
    rx_coop_exec *coop;
} rps_cli_t;

void rps_cli_machine_init(rx_fsm_machine *machine, rps_cli_t *cli);
void rps_cli_print_help(void);
void rps_cli_print_status(const rps_cli_t *cli);
