#include "rps/rps_app.h"
#include "rps/rps_command.h"
#include "rps_test.h"

static void apply_op(raft_application_t *app, rps_operation_t *op, int seq) {
    raft_command_t cmd;
    ASSERT_EQ_INT(0, rps_command_encode(op, seq, &cmd));
    app->apply(app->user, &cmd);
}

int main(void) {
    rps_app_t state;
    rps_app_t restored;
    raft_application_t app = rps_app_make(&state);
    raft_application_t restored_app = rps_app_make(&restored);
    rps_operation_t op;
    char snapshot[4096];
    size_t snapshot_size;

    app.reload(app.user);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_PLAYER_JOINED;
    strcpy(op.player_id, "alice");
    apply_op(&app, &op, 1);
    apply_op(&app, &op, 2);
    ASSERT_EQ_INT(1, state.game.player_count);
    ASSERT_STREQ("", state.game.last_error);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_PLAYER_JOINED;
    strcpy(op.player_id, "bob");
    apply_op(&app, &op, 3);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_CONFIRM_PLAYERS;
    apply_op(&app, &op, 4);
    ASSERT_EQ_INT(RPS_STATE_CONFIGURING_GAME, rps_game_state(&state.game));

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_CONFIGURE;
    op.config = rps_game_default_config();
    op.config.target_score = 1;
    apply_op(&app, &op, 5);
    ASSERT_EQ_INT(RPS_STATE_ROUND_OPEN, rps_game_state(&state.game));

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_START_ROUND;
    apply_op(&app, &op, 6);
    apply_op(&app, &op, 60);
    ASSERT_EQ_INT(RPS_STATE_WAITING_COMMITS, rps_game_state(&state.game));
    ASSERT_EQ_INT(1, state.game.round_number);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_COMMIT_MOVE;
    strcpy(op.player_id, "alice");
    ASSERT_EQ_INT(0, rps_make_commitment("alice", RPS_MOVE_ROCK, "alice", op.commit_hash, sizeof(op.commit_hash)));
    apply_op(&app, &op, 7);
    apply_op(&app, &op, 70);
    ASSERT_EQ_INT(1, (int)state.game.commit_count);
    ASSERT_STREQ("", state.game.last_error);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_COMMIT_MOVE;
    strcpy(op.player_id, "bob");
    ASSERT_EQ_INT(0, rps_make_commitment("bob", RPS_MOVE_SCISSORS, "bob", op.commit_hash, sizeof(op.commit_hash)));
    apply_op(&app, &op, 8);

    ASSERT_EQ_INT(RPS_STATE_WAITING_REVEALS, rps_game_state(&state.game));

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_REVEAL_MOVE;
    strcpy(op.player_id, "alice");
    op.move = RPS_MOVE_ROCK;
    strcpy(op.nonce, "alice");
    apply_op(&app, &op, 10);
    apply_op(&app, &op, 100);
    ASSERT_EQ_INT(1, (int)state.game.reveal_count);
    ASSERT_STREQ("", state.game.last_error);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_REVEAL_MOVE;
    strcpy(op.player_id, "bob");
    op.move = RPS_MOVE_SCISSORS;
    strcpy(op.nonce, "bob");
    apply_op(&app, &op, 11);

    ASSERT_EQ_INT(RPS_STATE_GAME_OVER, rps_game_state(&state.game));
    ASSERT_STREQ("alice", state.game.winner);

    snapshot_size = app.snapshot(app.user, snapshot, sizeof(snapshot));
    ASSERT_TRUE(snapshot_size > 0);
    restored_app.restore_snapshot(restored_app.user, snapshot, snapshot_size);
    ASSERT_EQ_INT(RPS_STATE_GAME_OVER, rps_game_state(&restored.game));
    ASSERT_STREQ("alice", restored.game.winner);
    return 0;
}
