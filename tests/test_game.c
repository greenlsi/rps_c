#include "rps/rps_game.h"
#include "rps_test.h"

static void configure_two_player_game(rps_game_t *game, const rps_game_config_t *cfg) {
    ASSERT_EQ_INT(0, rps_game_add_player(game, "alice"));
    ASSERT_EQ_INT(0, rps_game_add_player(game, "bob"));
    ASSERT_EQ_INT(0, rps_game_confirm_players(game));
    ASSERT_EQ_INT(RPS_STATE_CONFIGURING_GAME, rps_game_state(game));
    ASSERT_EQ_INT(0, rps_game_configure(game, cfg));
    ASSERT_EQ_INT(RPS_STATE_ROUND_OPEN, rps_game_state(game));
}

int main(void) {
    rps_game_t game;
    rps_game_config_t cfg = rps_game_default_config();
    char alice_hash[65];
    char bob_hash[65];

    cfg.target_score = 1;
    rps_game_init(&game);
    configure_two_player_game(&game, &cfg);
    ASSERT_EQ_INT(0, rps_game_start_round(&game));
    ASSERT_EQ_INT(0, rps_game_start_round(&game));
    ASSERT_EQ_INT(1, game.round_number);
    ASSERT_EQ_INT(RPS_STATE_WAITING_COMMITS, rps_game_state(&game));

    ASSERT_EQ_INT(0, rps_make_commitment("alice", RPS_MOVE_ROCK, "alice", alice_hash, sizeof(alice_hash)));
    ASSERT_EQ_INT(0, rps_make_commitment("bob", RPS_MOVE_SCISSORS, "bob", bob_hash, sizeof(bob_hash)));
    ASSERT_EQ_INT(0, rps_game_submit_commit(&game, "alice", alice_hash));
    ASSERT_EQ_INT(0, rps_game_submit_commit(&game, "alice", alice_hash));
    ASSERT_EQ_INT(1, (int)game.commit_count);
    ASSERT_STREQ("", game.last_error);
    ASSERT_EQ_INT(0, rps_game_submit_commit(&game, "bob", bob_hash));
    ASSERT_EQ_INT(RPS_STATE_WAITING_COMMITS, rps_game_state(&game));
    rps_game_tick(&game, 1);
    ASSERT_EQ_INT(RPS_STATE_WAITING_REVEALS, rps_game_state(&game));

    ASSERT_EQ_INT(0, rps_game_reveal(&game, "alice", RPS_MOVE_ROCK, "alice"));
    ASSERT_EQ_INT(0, rps_game_reveal(&game, "alice", RPS_MOVE_ROCK, "alice"));
    ASSERT_EQ_INT(1, (int)game.reveal_count);
    ASSERT_STREQ("", game.last_error);
    ASSERT_EQ_INT(0, rps_game_reveal(&game, "bob", RPS_MOVE_SCISSORS, "bob"));
    ASSERT_EQ_INT(RPS_STATE_RESOLVING_ROUND, rps_game_state(&game));
    rps_game_tick(&game, 3);
    ASSERT_EQ_INT(RPS_STATE_GAME_OVER, rps_game_state(&game));
    ASSERT_STREQ("alice", game.winner);
    ASSERT_EQ_INT(1, rps_game_score(&game, "alice"));
    ASSERT_EQ_INT(0, rps_game_score(&game, "bob"));

    cfg = rps_game_default_config();
    cfg.target_score = 1;
    cfg.reveal_timeout_ticks = 1;
    rps_game_init(&game);
    configure_two_player_game(&game, &cfg);
    ASSERT_EQ_INT(0, rps_game_start_round(&game));
    ASSERT_EQ_INT(0, rps_make_commitment("alice", RPS_MOVE_PAPER, "good", alice_hash, sizeof(alice_hash)));
    ASSERT_EQ_INT(0, rps_make_commitment("bob", RPS_MOVE_ROCK, "bob", bob_hash, sizeof(bob_hash)));
    ASSERT_EQ_INT(0, rps_game_submit_commit(&game, "alice", alice_hash));
    ASSERT_EQ_INT(0, rps_game_submit_commit(&game, "bob", bob_hash));
    rps_game_tick(&game, 1);
    ASSERT_EQ_INT(-1, rps_game_reveal(&game, "alice", RPS_MOVE_SCISSORS, "bad"));
    ASSERT_TRUE(strstr(game.last_error, "invalid reveal") != NULL);
    rps_game_tick(&game, 3);
    ASSERT_EQ_INT(RPS_STATE_ROUND_FINISHED, rps_game_state(&game));
    ASSERT_STREQ("", game.winner);

    return 0;
}
