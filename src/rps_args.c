#include "rps/rps_args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s --id NAME [--player PLAYER] [--port PORT] [--host HOST] [--data DIR]\n"
        "          [--member ID] ... [--peer ID=HOST:PORT] ... [--join HOST:PORT]\n",
        argv0);
    exit(1);
}

static int parse_host_port(const char *arg, char *host, size_t hostsz, int *port) {
    const char *colon = strrchr(arg, ':');
    size_t len;
    if (!colon) return -1;
    len = (size_t)(colon - arg);
    if (len == 0 || len >= hostsz) return -1;
    strncpy(host, arg, len);
    host[len] = '\0';
    *port = atoi(colon + 1);
    return *port > 0 ? 0 : -1;
}

static int parse_peer(const char *arg, char *id, char *host, int *port) {
    const char *eq = strchr(arg, '=');
    size_t len;
    if (!eq) return -1;
    len = (size_t)(eq - arg);
    if (len == 0 || len >= RAFT_MAX_ID) return -1;
    strncpy(id, arg, len);
    id[len] = '\0';
    return parse_host_port(eq + 1, host, 256, port);
}

rps_args_t rps_args_parse(int argc, char **argv) {
    rps_args_t args;
    int i;
    memset(&args, 0, sizeof(args));
    strncpy(args.host, RPS_DEFAULT_HOST, sizeof(args.host) - 1);
    strncpy(args.data_dir, RPS_DEFAULT_DATA_DIR, sizeof(args.data_dir) - 1);
    snprintf(args.player_id, sizeof(args.player_id), "p%d", (int)getpid());

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            strncpy(args.node_id, argv[++i], RAFT_MAX_ID - 1);
        } else if (strcmp(argv[i], "--player") == 0 && i + 1 < argc) {
            strncpy(args.player_id, argv[++i], sizeof(args.player_id) - 1);
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            strncpy(args.host, argv[++i], sizeof(args.host) - 1);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            args.port = atoi(argv[++i]);
            args.port_supplied = 1;
        } else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            strncpy(args.data_dir, argv[++i], sizeof(args.data_dir) - 1);
        } else if (strcmp(argv[i], "--member") == 0 && i + 1 < argc) {
            if (args.member_count < RAFT_MAX_NODES)
                strncpy(args.initial_members[args.member_count++], argv[++i], RAFT_MAX_ID - 1);
        } else if (strcmp(argv[i], "--peer") == 0 && i + 1 < argc) {
            if (args.peer_count < RAFT_MAX_PEERS) {
                size_t idx = args.peer_count;
                if (parse_peer(argv[++i], args.peer_ids[idx], args.peer_hosts[idx], &args.peer_ports[idx]) != 0) {
                    fprintf(stderr, "bad --peer: %s\n", argv[i]);
                    usage(argv[0]);
                }
                args.peer_count++;
            }
        } else if (strcmp(argv[i], "--join") == 0 && i + 1 < argc) {
            if (parse_host_port(argv[++i], args.join_host, sizeof(args.join_host), &args.join_port) != 0) {
                fprintf(stderr, "bad --join: %s\n", argv[i]);
                usage(argv[0]);
            }
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
        }
    }
    if (!args.node_id[0]) {
        fprintf(stderr, "--id is required\n");
        usage(argv[0]);
    }
    if (args.join_port > 0 && (!args.port_supplied || args.port <= 0)) {
        fprintf(stderr, "--join requires --port\n");
        usage(argv[0]);
    }
    return args;
}
