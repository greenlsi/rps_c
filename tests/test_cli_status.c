#include "rps/rps_cli.h"
#include "rps_test.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void capture_status(const rps_cli_t *cli, char *out, size_t out_size) {
    FILE *tmp = tmpfile();
    int saved_stdout = dup(STDOUT_FILENO);
    long n;
    ASSERT_TRUE(tmp != NULL);
    ASSERT_TRUE(saved_stdout >= 0);
    ASSERT_EQ_INT(0, fflush(stdout));
    ASSERT_TRUE(dup2(fileno(tmp), STDOUT_FILENO) >= 0);
    rps_cli_print_status(cli);
    ASSERT_EQ_INT(0, fflush(stdout));
    ASSERT_TRUE(dup2(saved_stdout, STDOUT_FILENO) >= 0);
    close(saved_stdout);
    rewind(tmp);
    n = fread(out, 1, out_size - 1, tmp);
    out[n] = '\0';
    fclose(tmp);
}

int main(void) {
    rps_cli_t cli;
    raft_node_t node;
    raft_tcp_transport_t tcp;
    rps_app_t app_state;
    char out[4096];

    memset(&cli, 0, sizeof(cli));
    memset(&node, 0, sizeof(node));
    memset(&tcp, 0, sizeof(tcp));
    memset(&app_state, 0, sizeof(app_state));

    strncpy(cli.node_id, "n1", sizeof(cli.node_id) - 1);
    strncpy(cli.player_id, "alice", sizeof(cli.player_id) - 1);
    cli.node = &node;
    cli.tcp = &tcp;
    cli.app = &app_state;

    strncpy(node.config.node_id, "n1", sizeof(node.config.node_id) - 1);
    strncpy(node.config.peers[node.config.peer_count++], "n2", RAFT_MAX_ID - 1);
    node.machine.state = RAFT_ROLE_LEADER;
    node.running = 1;
    node.current_term = 3;
    strncpy(node.leader_id, "n1", sizeof(node.leader_id) - 1);
    node.log_count = 4;
    node.commit_index = 4;
    node.last_applied = 4;
    node.match_index[0] = 3;
    node.next_index[0] = 4;
    strncpy(node.config_state.old_members[node.config_state.old_count++], "n1", RAFT_MAX_ID - 1);
    strncpy(node.config_state.old_members[node.config_state.old_count++], "n2", RAFT_MAX_ID - 1);

    strncpy(tcp.peers[tcp.peer_count].id, "n2", RAFT_MAX_ID - 1);
    strncpy(tcp.peers[tcp.peer_count].host, "127.0.0.1", sizeof(tcp.peers[tcp.peer_count].host) - 1);
    tcp.peers[tcp.peer_count].port = 7402;
    tcp.peer_count++;

    rps_game_init(&app_state.game);
    rps_game_add_player(&app_state.game, "alice");

    capture_status(&cli, out, sizeof(out));

    ASSERT_TRUE(strstr(out, "app:\n") != NULL);
    ASSERT_TRUE(strstr(out, "raft:\n") != NULL);
    ASSERT_TRUE(strstr(out, "replication:\n") != NULL);
    ASSERT_TRUE(strstr(out, "score: alice=0") != NULL);
    ASSERT_TRUE(strstr(out, "n2 match=3 next=4 lag=1 behind") != NULL);

    return 0;
}
