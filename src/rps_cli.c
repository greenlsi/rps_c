#include "rps/rps_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static const char *role_name(int state) {
    switch (state) {
        case RAFT_ROLE_LEADER: return "LEADER";
        case RAFT_ROLE_CANDIDATE: return "CANDIDATE";
        default: return "FOLLOWER";
    }
}

static int member_contains(const char members[][RAFT_MAX_ID], size_t count, const char *id) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (strcmp(members[i], id) == 0) return 1;
    return 0;
}

static int find_peer_address(const rps_cli_t *cli, const char *id, char *host, int *port) {
    size_t i;
    if (strcmp(id, cli->node->config.node_id) == 0) {
        if (!raft_tcp_transport_is_listening(cli->tcp)) return -1;
        strncpy(host, cli->tcp->own_host, 255);
        host[255] = '\0';
        *port = cli->tcp->listen_port;
        return 0;
    }
    for (i = 0; i < cli->tcp->peer_count; ++i) {
        if (strcmp(cli->tcp->peers[i].id, id) == 0) {
            strncpy(host, cli->tcp->peers[i].host, 255);
            host[255] = '\0';
            *port = cli->tcp->peers[i].port;
            return 0;
        }
    }
    return -1;
}

static int find_leader_address(const rps_cli_t *cli, char *host, int *port) {
    if (!cli->node->leader_id[0]) return -1;
    return find_peer_address(cli, cli->node->leader_id, host, port);
}

static int forward_to_leader(rps_cli_t *cli, const raft_command_t *cmd) {
    char host[256];
    int port;
    if (find_leader_address(cli, host, &port) != 0) {
        puts("leader unknown");
        return -1;
    }
    if (raft_tcp_transport_forward_cmd(cli->tcp, host, port, cmd) != 0) {
        puts("forward failed");
        return -1;
    }
    puts("forwarded");
    return 0;
}

static int submit_command(rps_cli_t *cli, rps_operation_t *op) {
    raft_command_t cmd;
    cli->sequence++;
    if (rps_command_encode(op, cli->sequence, &cmd) != 0) return -1;
    if (cli->node->machine.state == RAFT_ROLE_LEADER) {
        if (raft_node_submit_command(cli->node, &cmd) == 0) {
            puts("queued");
            return 0;
        }
        puts("queue failed");
        return -1;
    }
    return forward_to_leader(cli, &cmd);
}

static void print_scores(const rps_game_t *game) {
    size_t i;
    printf("score:");
    for (i = 0; i < game->player_count; ++i)
        printf(" %s=%d", game->players[i], game->scores[i]);
    puts(game->player_count ? "" : " (empty)");
}

static void cmd_status(const rps_cli_t *cli) {
    const raft_node_t *n = cli->node;
    const rps_game_t *g = &cli->app->game;
    size_t i;
    printf("id=%s player=%s running=%s role=%s term=%d leader=%s commit=%d applied=%d log=%zu\n",
           n->config.node_id, cli->player_id, n->running ? "yes" : "no",
           role_name(n->machine.state), n->current_term,
           n->leader_id[0] ? n->leader_id : "unknown", n->commit_index,
           n->last_applied, n->log_count);
    printf("members old=[");
    for (i = 0; i < n->config_state.old_count; ++i)
        printf("%s%s", n->config_state.old_members[i], i + 1 < n->config_state.old_count ? "," : "");
    printf("] new=[");
    for (i = 0; i < n->config_state.new_count; ++i)
        printf("%s%s", n->config_state.new_members[i], i + 1 < n->config_state.new_count ? "," : "");
    puts("]");
    printf("peers:");
    for (i = 0; i < cli->tcp->peer_count; ++i)
        printf(" %s=%s:%d", cli->tcp->peers[i].id, cli->tcp->peers[i].host, cli->tcp->peers[i].port);
    puts(cli->tcp->peer_count ? "" : " (none)");
    printf("game=%s round=%d players=%zu commits=%zu reveals=%zu winner=%s\n",
           rps_state_name(rps_game_state(g)), g->round_number, g->player_count,
           g->commit_count, g->reveal_count, g->winner[0] ? g->winner : "none");
    print_scores(g);
    if (g->last_announcement[0]) printf("last: %s\n", g->last_announcement);
    if (g->last_error[0]) printf("error: %s\n", g->last_error);
}

static void cmd_log(const rps_cli_t *cli) {
    const raft_node_t *n = cli->node;
    size_t i;
    printf("term=%d role=%s log=%zu snapshot=%d commit=%d applied=%d\n",
           n->current_term, role_name(n->machine.state), n->log_count,
           n->snapshot_last_included_index, n->commit_index, n->last_applied);
    for (i = 0; i < n->log_count; ++i) {
        const raft_log_entry_t *e = &n->log[i];
        printf("[%d] %s term=%d op=%s key=%s value=%s\n",
               e->index, e->index <= n->commit_index ? "COMMITTED  " : "UNCOMMITTED",
               e->term, e->command.op, e->command.key, e->command.value);
    }
}

static int parse_optional_timeout(const char *text) {
    if (text == NULL || strcmp(text, "none") == 0 || strcmp(text, "inf") == 0 || strcmp(text, "-") == 0)
        return -1;
    return atoi(text);
}

static void cmd_config(rps_cli_t *cli, char **tok, int n) {
    rps_operation_t op;
    rps_game_config_t cfg = rps_game_default_config();
    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_CONFIGURE;
    if (n > 1) cfg.target_score = atoi(tok[1]);
    if (n > 2) cfg.max_rounds = atoi(tok[2]);
    if (n > 3) cfg.commit_timeout_ticks = parse_optional_timeout(tok[3]);
    if (n > 4) cfg.reveal_timeout_ticks = parse_optional_timeout(tok[4]);
    op.config = cfg;
    submit_command(cli, &op);
}

static void cmd_commit(rps_cli_t *cli, char **tok, int n) {
    rps_operation_t op;
    rps_move_t move;
    char nonce[RPS_MAX_TEXT];
    if (n < 2 || rps_parse_move(tok[1], &move) != 0) {
        puts("usage: commit piedra|papel|tijera [nonce]");
        return;
    }
    snprintf(nonce, sizeof(nonce), "%s", n > 2 ? tok[2] : cli->player_id);
    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_COMMIT_MOVE;
    strncpy(op.player_id, cli->player_id, sizeof(op.player_id) - 1);
    rps_make_commitment(cli->player_id, move, nonce, op.commit_hash, sizeof(op.commit_hash));
    submit_command(cli, &op);
    printf("nonce=%s\n", nonce);
}

static void cmd_reveal(rps_cli_t *cli, char **tok, int n) {
    rps_operation_t op;
    rps_move_t move;
    if (n < 3 || rps_parse_move(tok[1], &move) != 0) {
        puts("usage: reveal piedra|papel|tijera NONCE");
        return;
    }
    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_REVEAL_MOVE;
    strncpy(op.player_id, cli->player_id, sizeof(op.player_id) - 1);
    op.move = move;
    strncpy(op.nonce, tok[2], sizeof(op.nonce) - 1);
    submit_command(cli, &op);
}

void rps_cli_print_help(void) {
    puts("Commands:");
    puts("  status");
    puts("  join PLAYER");
    puts("  confirm");
    puts("  config [target_score] [max_rounds] [commit_timeout] [reveal_timeout]");
    puts("  start");
    puts("  commit piedra|papel|tijera [nonce]");
    puts("  reveal piedra|papel|tijera NONCE");
    puts("  score");
    puts("  log");
    puts("  help");
    puts("  quit");
}

static void dispatch(rps_cli_t *cli, char *line) {
    char *tok[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    char *p = line;
    char *nl = strchr(line, '\n');
    int n = 0;
    rps_operation_t op;
    if (nl) *nl = '\0';
    while (n < 6 && (tok[n] = strtok(p, " \t")) != NULL) { p = NULL; ++n; }
    if (n == 0) return;
    memset(&op, 0, sizeof(op));

    if (strcmp(tok[0], "status") == 0) cmd_status(cli);
    else if (strcmp(tok[0], "join") == 0) {
        if (n < 2) puts("usage: join PLAYER");
        else { op.kind = RPS_OP_PLAYER_JOINED; strncpy(op.player_id, tok[1], sizeof(op.player_id) - 1); submit_command(cli, &op); }
    } else if (strcmp(tok[0], "confirm") == 0) {
        op.kind = RPS_OP_CONFIRM_PLAYERS; submit_command(cli, &op);
    } else if (strcmp(tok[0], "config") == 0) cmd_config(cli, tok, n);
    else if (strcmp(tok[0], "start") == 0) {
        op.kind = RPS_OP_START_ROUND; submit_command(cli, &op);
    } else if (strcmp(tok[0], "commit") == 0) cmd_commit(cli, tok, n);
    else if (strcmp(tok[0], "reveal") == 0) cmd_reveal(cli, tok, n);
    else if (strcmp(tok[0], "score") == 0) print_scores(&cli->app->game);
    else if (strcmp(tok[0], "log") == 0) cmd_log(cli);
    else if (strcmp(tok[0], "help") == 0) rps_cli_print_help();
    else if (strcmp(tok[0], "quit") == 0 || strcmp(tok[0], "exit") == 0) cli->quit_requested = 1;
    else printf("unknown: %s\n", tok[0]);
}

static void cli_latch(rx_fsm_context *ctx, void *user) {
    rps_cli_t *cli = (rps_cli_t *)user;
    fd_set rfds;
    struct timeval tv = {0, 0};
    (void)ctx;
    cli->has_line = 0;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0) {
        if (fgets(cli->line, sizeof(cli->line), stdin)) cli->has_line = 1;
        else cli->quit_requested = 1;
    }
    cli->fwd_cmd_count += raft_tcp_transport_recv_fwd_cmds(
        cli->tcp, cli->fwd_cmds + cli->fwd_cmd_count, RAFT_MAX_QUEUE - cli->fwd_cmd_count);
    cli->pending_join_count += raft_tcp_transport_recv_joins(
        cli->tcp, cli->pending_joins + cli->pending_join_count, RAFT_MAX_NODES - cli->pending_join_count);
}

static int guard_line(const rx_fsm_context *ctx, void *user) {
    (void)ctx; return ((rps_cli_t *)user)->has_line;
}

static void action_execute(rx_fsm_context *ctx, void *user) {
    rps_cli_t *cli = (rps_cli_t *)user;
    (void)ctx;
    dispatch(cli, cli->line);
    cli->prompt_needed = 1;
}

static size_t collect_members(const rps_cli_t *cli, char out[RAFT_MAX_NODES][RAFT_MAX_ID]) {
    size_t count = 0, i;
    for (i = 0; i < cli->node->config_state.old_count && count < RAFT_MAX_NODES; ++i)
        if (!member_contains(out, count, cli->node->config_state.old_members[i]))
            strncpy(out[count++], cli->node->config_state.old_members[i], RAFT_MAX_ID - 1);
    for (i = 0; i < cli->node->config_state.new_count && count < RAFT_MAX_NODES; ++i)
        if (!member_contains(out, count, cli->node->config_state.new_members[i]))
            strncpy(out[count++], cli->node->config_state.new_members[i], RAFT_MAX_ID - 1);
    return count;
}

static void process_joins(rps_cli_t *cli) {
    size_t i = 0, j;
    while (i < cli->pending_join_count) {
        raft_tcp_join_req_t *jr = &cli->pending_joins[i];
        raft_tcp_transport_add_peer(cli->tcp, jr->node_id, jr->host, jr->port);
        if (jr->forwarded == RAFT_JOIN_ORIGINAL) {
            raft_tcp_transport_flood_join(cli->tcp, jr);
            raft_tcp_transport_announce_self(cli->tcp, jr->host, jr->port);
        } else if (jr->forwarded == RAFT_JOIN_FORWARDED) {
            raft_tcp_transport_announce_self(cli->tcp, jr->host, jr->port);
        }
        if (cli->node->machine.state == RAFT_ROLE_LEADER) {
            char members[RAFT_MAX_NODES][RAFT_MAX_ID];
            size_t count = collect_members(cli, members);
            if (!member_contains(members, count, jr->node_id) && count < RAFT_MAX_NODES) {
                strncpy(members[count++], jr->node_id, RAFT_MAX_ID - 1);
                raft_node_request_membership_change(cli->node, members, count);
            }
            for (j = i + 1; j < cli->pending_join_count; ++j) cli->pending_joins[j - 1] = cli->pending_joins[j];
            cli->pending_join_count--;
        } else {
            i++;
        }
    }
}

static void cli_dump(rx_fsm_context *ctx, void *user) {
    rps_cli_t *cli = (rps_cli_t *)user;
    size_t i;
    (void)ctx;
    if (cli->quit_requested) {
        rx_coop_exec_stop(cli->coop);
        return;
    }
    for (i = 0; i < cli->fwd_cmd_count; ++i)
        if (cli->node->machine.state == RAFT_ROLE_LEADER)
            raft_node_submit_command(cli->node, &cli->fwd_cmds[i]);
    cli->fwd_cmd_count = 0;
    process_joins(cli);
    if (cli->node->machine.state != cli->last_role || cli->node->current_term != cli->last_term ||
        cli->node->commit_index != cli->last_commit_index) {
        fprintf(stderr, "\n[%s] role=%s term=%d leader=%s commit=%d game=%s\n",
                cli->node_id, role_name(cli->node->machine.state), cli->node->current_term,
                cli->node->leader_id[0] ? cli->node->leader_id : "unknown",
                cli->node->commit_index, rps_state_name(rps_game_state(&cli->app->game)));
        cli->last_role = cli->node->machine.state;
        cli->last_term = cli->node->current_term;
        cli->last_commit_index = cli->node->commit_index;
        cli->prompt_needed = 1;
    }
    if (cli->prompt_needed) {
        printf("rps> ");
        fflush(stdout);
        cli->prompt_needed = 0;
    }
}

static const rx_fsm_transition cli_transitions[] = {
    {RPS_CLI_READY, RPS_CLI_READY, guard_line, action_execute}
};

void rps_cli_machine_init(rx_fsm_machine *machine, rps_cli_t *cli) {
    rx_fsm_machine_init(machine, "rps_cli", RPS_CLI_READY,
                        cli_transitions, sizeof(cli_transitions) / sizeof(cli_transitions[0]),
                        cli, cli_latch, cli_dump);
}
