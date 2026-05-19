#include "rps/rps_app.h"

#include <stdio.h>
#include <string.h>

static int contains_key(char keys[][RAFT_MAX_KEY], size_t count, const char *key) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (strcmp(keys[i], key) == 0) return 1;
    return 0;
}

static void remember_key(char keys[][RAFT_MAX_KEY], size_t *count, const char *key) {
    if (*count >= RAFT_MAX_LOG) return;
    strncpy(keys[*count], key, RAFT_MAX_KEY - 1);
    (*count)++;
}

static void apply_operation(rps_app_t *state, const rps_operation_t *op) {
    switch (op->kind) {
        case RPS_OP_PLAYER_JOINED:
            rps_game_add_player(&state->game, op->player_id);
            break;
        case RPS_OP_CONFIRM_PLAYERS:
            rps_game_confirm_players(&state->game);
            break;
        case RPS_OP_CONFIGURE:
            rps_game_configure(&state->game, &op->config);
            break;
        case RPS_OP_START_ROUND:
            rps_game_start_round(&state->game);
            break;
        case RPS_OP_COMMIT_MOVE:
            rps_game_submit_commit(&state->game, op->player_id, op->commit_hash);
            break;
        case RPS_OP_REVEAL_MOVE:
            rps_game_reveal(&state->game, op->player_id, op->move, op->nonce);
            break;
        case RPS_OP_PEER_SET:
            if (state->peer_set) state->peer_set(state->peer_set_user, op->player_id, op->peer_value);
            break;
        default:
            break;
    }
}

static void drain_automatic_transitions(rps_game_t *game) {
    int i;
    for (i = 0; i < 8; ++i) {
        rps_state_t before = rps_game_state(game);
        rps_game_tick(game, 1);
        if (rps_game_state(game) == before) return;
    }
}

static void app_apply(void *user, const raft_command_t *command) {
    rps_app_t *state = (rps_app_t *)user;
    rps_operation_t op;
    if (contains_key(state->applied_keys, state->applied_key_count, command->key)) return;
    if (rps_command_decode(command, &op) != 0) return;
    if (op.operation_id[0] && contains_key(state->applied_operation_ids, state->applied_operation_id_count, op.operation_id)) {
        remember_key(state->applied_keys, &state->applied_key_count, command->key);
        return;
    }
    apply_operation(state, &op);
    drain_automatic_transitions(&state->game);
    remember_key(state->applied_keys, &state->applied_key_count, command->key);
    if (op.operation_id[0]) remember_key(state->applied_operation_ids, &state->applied_operation_id_count, op.operation_id);
    if (state->applied_command_count < RAFT_MAX_LOG)
        state->applied_commands[state->applied_command_count++] = *command;
}

static void app_reload(void *user) {
    rps_app_t *state = (rps_app_t *)user;
    rps_peer_set_fn peer_set = state->peer_set;
    void *peer_user = state->peer_set_user;
    memset(state, 0, sizeof(*state));
    state->peer_set = peer_set;
    state->peer_set_user = peer_user;
    rps_game_init(&state->game);
}

static size_t app_snapshot(void *user, void *buf, size_t buf_size) {
    rps_app_t *state = (rps_app_t *)user;
    char *out = (char *)buf;
    size_t used = 0;
    size_t i;
    for (i = 0; i < state->applied_command_count; ++i) {
        const raft_command_t *cmd = &state->applied_commands[i];
        int n = snprintf(out + used, used < buf_size ? buf_size - used : 0,
                         "%s\t%s\t%s\n", cmd->op, cmd->key, cmd->value);
        if (n < 0) return used;
        if (used + (size_t)n >= buf_size) return used;
        used += (size_t)n;
    }
    return used;
}

static void app_restore_snapshot(void *user, const void *buf, size_t size) {
    rps_app_t *state = (rps_app_t *)user;
    char snapshot[RAFT_MAX_SNAPSHOT_SIZE + 1];
    char *line;
    char *save = NULL;
    size_t copy = size < RAFT_MAX_SNAPSHOT_SIZE ? size : RAFT_MAX_SNAPSHOT_SIZE;
    app_reload(user);
    memcpy(snapshot, buf, copy);
    snapshot[copy] = '\0';
    line = strtok_r(snapshot, "\n", &save);
    while (line) {
        char *op = strtok(line, "\t");
        char *key = strtok(NULL, "\t");
        char *value = strtok(NULL, "\t");
        raft_command_t cmd;
        if (op && key) {
            memset(&cmd, 0, sizeof(cmd));
            strncpy(cmd.op, op, sizeof(cmd.op) - 1);
            strncpy(cmd.key, key, sizeof(cmd.key) - 1);
            if (value) strncpy(cmd.value, value, sizeof(cmd.value) - 1);
            app_apply(state, &cmd);
        }
        line = strtok_r(NULL, "\n", &save);
    }
}

raft_application_t rps_app_make_with_peers(rps_app_t *state, rps_peer_set_fn peer_set, void *peer_set_user) {
    raft_application_t app;
    memset(state, 0, sizeof(*state));
    state->peer_set = peer_set;
    state->peer_set_user = peer_set_user;
    rps_game_init(&state->game);
    memset(&app, 0, sizeof(app));
    app.apply = app_apply;
    app.reload = app_reload;
    app.snapshot = app_snapshot;
    app.restore_snapshot = app_restore_snapshot;
    app.user = state;
    return app;
}

raft_application_t rps_app_make(rps_app_t *state) {
    return rps_app_make_with_peers(state, NULL, NULL);
}
