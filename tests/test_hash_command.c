#include "rps/rps_command.h"
#include "rps/rps_game.h"
#include "rps_test.h"

int main(void) {
    char hash[65];
    rps_operation_t op;
    raft_command_t cmd;

    ASSERT_EQ_INT(0, rps_make_commitment("alice", RPS_MOVE_ROCK, "a1", hash, sizeof(hash)));
    ASSERT_STREQ("4d936656be1127ebc333b0cc1fa36668cda69b303bc21501127c7196ff670333", hash);

    memset(&op, 0, sizeof(op));
    op.kind = RPS_OP_CONFIGURE;
    op.config.target_score = 1;
    op.config.max_rounds = 9;
    op.config.commit_timeout_ticks = -1;
    op.config.reveal_timeout_ticks = 2;
    ASSERT_EQ_INT(0, rps_command_encode(&op, 7, &cmd));
    ASSERT_STREQ("rps.configure", cmd.op);
    ASSERT_STREQ("00000007", cmd.key);

    strncpy(op.operation_id, "n2-00000007", sizeof(op.operation_id) - 1);
    ASSERT_EQ_INT(0, rps_command_encode(&op, 7, &cmd));
    ASSERT_STREQ("n2-00000007", cmd.key);

    memset(&op, 0, sizeof(op));
    ASSERT_EQ_INT(0, rps_command_decode(&cmd, &op));
    ASSERT_EQ_INT(RPS_OP_CONFIGURE, op.kind);
    ASSERT_STREQ("n2-00000007", op.operation_id);
    ASSERT_EQ_INT(1, op.config.target_score);
    ASSERT_EQ_INT(9, op.config.max_rounds);
    ASSERT_EQ_INT(-1, op.config.commit_timeout_ticks);
    ASSERT_EQ_INT(2, op.config.reveal_timeout_ticks);

    return 0;
}
