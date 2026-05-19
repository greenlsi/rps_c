#include "rps/rps_args.h"
#include "rps/rps_app.h"
#include "rps/rps_cli.h"

#include "raft/raft_tcp_transport.h"
#include "rxnet/coop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RPS_TICK_US 10000L
#define RPS_ELECTION_TIMEOUT_MS 500
#define RPS_HEARTBEAT_MS 100

typedef struct {
    rx_fsm_runtime runtime;
    raft_cluster_t cluster;
    raft_tcp_transport_t tcp;
    rx_coop_exec coop;
    rx_fsm_machine cli_machine;
    raft_node_t *node;
    rps_app_t app_state;
    rps_cli_t cli;
} rps_process_t;

static int parse_host_port(const char *arg, char *host, size_t hostsz, int *port) {
    const char *colon = strrchr(arg, ':');
    size_t hlen;
    if (!colon) return -1;
    hlen = (size_t)(colon - arg);
    if (hlen == 0 || hlen >= hostsz) return -1;
    strncpy(host, arg, hlen);
    host[hlen] = '\0';
    *port = atoi(colon + 1);
    return *port > 0 ? 0 : -1;
}

static void register_peer_from_app(void *user, const char *node_id, const char *address) {
    rps_process_t *proc = (rps_process_t *)user;
    char host[256];
    int port;
    if (strcmp(node_id, proc->tcp.own_id) == 0) return;
    if (parse_host_port(address, host, sizeof(host), &port) == 0)
        raft_tcp_transport_add_peer(&proc->tcp, node_id, host, port);
}

static void configure_transport(rps_process_t *proc, const rps_args_t *args) {
    size_t i;
    char node_dir[512];
    raft_tcp_transport_init(&proc->tcp, args->port);
    raft_tcp_transport_set_self(&proc->tcp, args->node_id, args->host);
    snprintf(node_dir, sizeof(node_dir), "%s/%s", args->data_dir, args->node_id);
    raft_tcp_transport_set_data_dir(&proc->tcp, node_dir);
    for (i = 0; i < args->peer_count; ++i)
        raft_tcp_transport_add_peer(&proc->tcp, args->peer_ids[i], args->peer_hosts[i], args->peer_ports[i]);
}

static void make_raft_config(raft_node_config_t *cfg, const rps_args_t *args) {
    size_t i;
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->node_id, args->node_id, RAFT_MAX_ID - 1);
    cfg->election_timeout_ms = RPS_ELECTION_TIMEOUT_MS;
    cfg->heartbeat_interval_ms = RPS_HEARTBEAT_MS;
    if (args->join_port > 0) cfg->learner = 1;
    for (i = 0; i < args->peer_count; ++i)
        strncpy(cfg->peers[cfg->peer_count++], args->peer_ids[i], RAFT_MAX_ID - 1);
    for (i = 0; i < args->member_count; ++i)
        strncpy(cfg->initial_members[cfg->initial_member_count++], args->initial_members[i], RAFT_MAX_ID - 1);
}

static int setup(rps_process_t *proc, const rps_args_t *args) {
    raft_node_config_t cfg;
    raft_application_t app;
    memset(proc, 0, sizeof(*proc));
    configure_transport(proc, args);
    if (proc->tcp.listen_port > 0 && raft_tcp_transport_start(&proc->tcp) != 0) {
        fprintf(stderr, "failed to listen on port %d\n", proc->tcp.listen_port);
        return -1;
    }
    rx_fsm_runtime_init(&proc->runtime, 2);
    raft_cluster_init(&proc->cluster, &proc->runtime);
    raft_cluster_enable_realtime_clock(&proc->cluster);
    make_raft_config(&cfg, args);
    app = rps_app_make_with_peers(&proc->app_state, register_peer_from_app, proc);
    proc->node = raft_cluster_add_node(&proc->cluster, &cfg, args->data_dir, &app, RPS_TICK_US);
    if (!proc->node) {
        fputs("raft_cluster_add_node failed\n", stderr);
        return -1;
    }
    proc->node->transport = raft_tcp_transport_make(&proc->tcp);

    memset(&proc->cli, 0, sizeof(proc->cli));
    strncpy(proc->cli.node_id, args->node_id, RAFT_MAX_ID - 1);
    strncpy(proc->cli.player_id, args->player_id, sizeof(proc->cli.player_id) - 1);
    proc->cli.node = proc->node;
    proc->cli.tcp = &proc->tcp;
    proc->cli.app = &proc->app_state;
    proc->cli.coop = &proc->coop;
    proc->cli.prompt_needed = 1;
    proc->cli.last_role = proc->node->machine.state;
    proc->cli.last_term = proc->node->current_term;
    proc->cli.last_commit_index = proc->node->commit_index;
    rps_cli_machine_init(&proc->cli_machine, &proc->cli);
    if (rx_fsm_runtime_add_machine(&proc->runtime, &proc->cli_machine, RPS_TICK_US, 0) != 0) {
        fputs("failed to add CLI machine\n", stderr);
        return -1;
    }
    rx_coop_exec_init(&proc->coop);
    if (rx_coop_exec_add(&proc->coop, &proc->runtime.runtime) != 0) {
        fputs("rx_coop_exec_add failed\n", stderr);
        return -1;
    }
    return 0;
}

static void request_join(rps_process_t *proc, const rps_args_t *args) {
    if (args->join_port <= 0) return;
    raft_tcp_transport_request_join(&proc->tcp, args->node_id, args->host, args->port,
                                    args->join_host, args->join_port);
}

static void teardown(rps_process_t *proc) {
    raft_tcp_transport_stop(&proc->tcp);
    raft_cluster_destroy(&proc->cluster);
    rx_fsm_runtime_free(&proc->runtime);
}

int main(int argc, char **argv) {
    rps_args_t args = rps_args_parse(argc, argv);
    static rps_process_t proc;
    if (setup(&proc, &args) != 0) return 1;
    request_join(&proc, &args);
    if (raft_tcp_transport_is_listening(&proc.tcp))
        printf("[%s] listening on %s:%d\n", args.node_id, proc.tcp.own_host, proc.tcp.listen_port);
    printf("player=%s\n", args.player_id);
    rps_cli_print_help();
    rx_coop_exec_run(&proc.coop);
    teardown(&proc);
    return 0;
}
