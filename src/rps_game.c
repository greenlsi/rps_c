#include "rps/rps_game.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    size_t datalen;
} sha256_ctx_t;

static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }
static uint32_t choose(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
static uint32_t majority(uint32_t a, uint32_t b, uint32_t c) { return (a & b) ^ (a & c) ^ (b & c); }
static uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

static const uint32_t k[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t data[64]) {
    uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;
    size_t i;
    for (i = 0; i < 16; ++i) {
        m[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | (uint32_t)data[i * 4 + 3];
    }
    for (; i < 64; ++i) m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + ep1(e) + choose(e, f, g) + k[i] + m[i];
        t2 = ep0(a) + majority(a, b, c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]) {
    size_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    for (i = 0; i < 8; ++i) ctx->data[63 - i] = (uint8_t)(ctx->bitlen >> (i * 8));
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]      = (uint8_t)(ctx->state[0] >> (24 - i * 8));
        hash[i + 4]  = (uint8_t)(ctx->state[1] >> (24 - i * 8));
        hash[i + 8]  = (uint8_t)(ctx->state[2] >> (24 - i * 8));
        hash[i + 12] = (uint8_t)(ctx->state[3] >> (24 - i * 8));
        hash[i + 16] = (uint8_t)(ctx->state[4] >> (24 - i * 8));
        hash[i + 20] = (uint8_t)(ctx->state[5] >> (24 - i * 8));
        hash[i + 24] = (uint8_t)(ctx->state[6] >> (24 - i * 8));
        hash[i + 28] = (uint8_t)(ctx->state[7] >> (24 - i * 8));
    }
}

static int player_index(const rps_game_t *game, const char *player_id) {
    size_t i;
    for (i = 0; i < game->player_count; ++i)
        if (strcmp(game->players[i], player_id) == 0) return (int)i;
    return -1;
}

static int commit_index(const rps_game_t *game, const char *player_id) {
    size_t i;
    for (i = 0; i < game->commit_count; ++i)
        if (strcmp(game->commits[i].player_id, player_id) == 0) return (int)i;
    return -1;
}

static int reveal_index(const rps_game_t *game, const char *player_id) {
    size_t i;
    for (i = 0; i < game->reveal_count; ++i)
        if (strcmp(game->reveals[i].player_id, player_id) == 0) return (int)i;
    return -1;
}

static void set_error(rps_game_t *game, const char *prefix, const char *value) {
    snprintf(game->last_error, sizeof(game->last_error), "%s%s", prefix, value);
}

static void set_announcement(rps_game_t *game, const char *prefix, const char *value) {
    snprintf(game->last_announcement, sizeof(game->last_announcement), "%s%s", prefix, value);
}

rps_game_config_t rps_game_default_config(void) {
    rps_game_config_t cfg;
    cfg.target_score = 3;
    cfg.max_rounds = 9;
    cfg.commit_timeout_ticks = -1;
    cfg.reveal_timeout_ticks = -1;
    return cfg;
}

const char *rps_state_name(rps_state_t state) {
    static const char *names[] = {
        "WAITING_PLAYERS", "CONFIGURING_GAME", "ROUND_OPEN", "WAITING_COMMITS",
        "WAITING_REVEALS", "RESOLVING_ROUND", "ROUND_FINISHED", "GAME_OVER"
    };
    return state >= 0 && (size_t)state < ARRAY_LEN(names) ? names[state] : "UNKNOWN";
}

const char *rps_move_name(rps_move_t move) {
    switch (move) {
        case RPS_MOVE_ROCK: return "rock";
        case RPS_MOVE_PAPER: return "paper";
        case RPS_MOVE_SCISSORS: return "scissors";
        default: return "unknown";
    }
}

int rps_parse_move(const char *text, rps_move_t *move) {
    if (strcmp(text, "rock") == 0 || strcmp(text, "piedra") == 0) *move = RPS_MOVE_ROCK;
    else if (strcmp(text, "paper") == 0 || strcmp(text, "papel") == 0) *move = RPS_MOVE_PAPER;
    else if (strcmp(text, "scissors") == 0 || strcmp(text, "tijera") == 0 || strcmp(text, "tijeras") == 0) *move = RPS_MOVE_SCISSORS;
    else return -1;
    return 0;
}

int rps_make_commitment(const char *player_id, rps_move_t move, const char *nonce,
                        char *out_hex, size_t out_size) {
    char payload[256];
    uint8_t digest[32];
    sha256_ctx_t ctx;
    size_t i;
    if (out_size < RPS_HASH_HEX) return -1;
    snprintf(payload, sizeof(payload), "%s:%d:%s", player_id, (int)move, nonce);
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)payload, strlen(payload));
    sha256_final(&ctx, digest);
    for (i = 0; i < 32; ++i) sprintf(out_hex + i * 2, "%02x", digest[i]);
    out_hex[64] = '\0';
    return 0;
}

static void latch_inputs(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    game->latched = game->inputs;
    memset(&game->inputs, 0, sizeof(game->inputs));
}

static void dump_outputs(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    game->tick++;
}

static int guard_player_joined(const rx_fsm_context *ctx, void *user) {
    (void)ctx; return ((rps_game_t *)user)->latched.has_joined_player;
}

static int guard_enough_players(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx; return game->latched.confirm_players && game->player_count >= 2;
}

static int guard_config(const rx_fsm_context *ctx, void *user) {
    (void)ctx; return ((rps_game_t *)user)->latched.config_committed;
}

static int guard_start(const rx_fsm_context *ctx, void *user) {
    (void)ctx; return ((rps_game_t *)user)->latched.round_started;
}

static int guard_commit(const rx_fsm_context *ctx, void *user) {
    (void)ctx; return ((rps_game_t *)user)->latched.has_commit;
}

static int guard_all_commits(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx; return game->commit_count == game->player_count && game->player_count > 0;
}

static int guard_commit_timeout(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    return game->config.commit_timeout_ticks > 0 && game->tick >= game->deadline_tick;
}

static int guard_all_reveals(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx; return game->reveal_count == game->commit_count && game->commit_count > 0;
}

static int guard_reveal_timeout(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    return game->config.reveal_timeout_ticks > 0 && game->tick >= game->deadline_tick;
}

static int guard_result(const rx_fsm_context *ctx, void *user) {
    (void)ctx; (void)user; return 1;
}

static int guard_next_round(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx; return game->winner[0] == '\0' && game->round_number < game->config.max_rounds;
}

static int guard_winner(const rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx; return game->winner[0] != '\0' || game->round_number >= game->config.max_rounds;
}

static void action_join(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    if (player_index(game, game->latched.player_id) >= 0) {
        return;
    }
    if (game->player_count >= RPS_MAX_PLAYERS) {
        set_error(game, "too many players: ", game->latched.player_id);
        return;
    }
    strncpy(game->players[game->player_count++], game->latched.player_id, RPS_MAX_ID - 1);
    set_announcement(game, "player joined: ", game->latched.player_id);
}

static void action_enter_config(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    size_t i;
    (void)ctx;
    for (i = 0; i < game->player_count; ++i) game->scores[i] = 0;
}

static void action_enter_round_open(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    game->commit_count = 0;
    game->reveal_count = 0;
}

static void action_enter_commits(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    game->round_number++;
    game->commit_count = 0;
    game->reveal_count = 0;
    if (game->config.commit_timeout_ticks > 0)
        game->deadline_tick = game->tick + game->config.commit_timeout_ticks + 1;
}

static void action_commit(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    if (player_index(game, game->latched.player_id) < 0) {
        set_error(game, "unknown player: ", game->latched.player_id);
        return;
    }
    if (commit_index(game, game->latched.player_id) >= 0) {
        set_error(game, "duplicate commit: ", game->latched.player_id);
        return;
    }
    if (game->commit_count >= RPS_MAX_PLAYERS) return;
    strncpy(game->commits[game->commit_count].player_id, game->latched.player_id, RPS_MAX_ID - 1);
    strncpy(game->commits[game->commit_count].hash, game->latched.commit_hash, RPS_HASH_HEX - 1);
    game->commit_count++;
    set_announcement(game, "commit received: ", game->latched.player_id);
}

static void action_enter_reveals(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    (void)ctx;
    if (game->config.reveal_timeout_ticks > 0)
        game->deadline_tick = game->tick + game->config.reveal_timeout_ticks + 1;
}

static int winning_move(rps_move_t move) {
    if (move == RPS_MOVE_ROCK) return RPS_MOVE_SCISSORS;
    if (move == RPS_MOVE_PAPER) return RPS_MOVE_ROCK;
    return RPS_MOVE_PAPER;
}

static void action_resolve(rx_fsm_context *ctx, void *user) {
    rps_game_t *game = (rps_game_t *)user;
    int has_move[3] = {0, 0, 0};
    int round_winner = -1;
    size_t i;
    (void)ctx;
    for (i = 0; i < game->reveal_count; ++i) has_move[(int)game->reveals[i].move] = 1;
    if (game->reveal_count >= 2 && !(has_move[0] && has_move[1] && has_move[2]) &&
        !(has_move[0] + has_move[1] + has_move[2] == 1)) {
        rps_move_t wm = RPS_MOVE_ROCK;
        for (i = 0; i < 3; ++i) if (has_move[i] && has_move[winning_move((rps_move_t)i)]) wm = (rps_move_t)i;
        for (i = 0; i < game->reveal_count; ++i) {
            if (game->reveals[i].move == wm) {
                int pidx = player_index(game, game->reveals[i].player_id);
                if (round_winner >= 0) { round_winner = -1; break; }
                round_winner = pidx;
            }
        }
    }
    if (round_winner >= 0) {
        game->scores[round_winner]++;
        snprintf(game->last_announcement, sizeof(game->last_announcement),
                 "round %d winner: %s", game->round_number, game->players[round_winner]);
        if (game->scores[round_winner] >= game->config.target_score)
            strncpy(game->winner, game->players[round_winner], sizeof(game->winner) - 1);
    } else {
        snprintf(game->last_announcement, sizeof(game->last_announcement),
                 "round %d result: draw", game->round_number);
    }
    if (game->winner[0] == '\0' && game->round_number >= game->config.max_rounds) {
        int high = -1, leader = -1, tie = 0;
        for (i = 0; i < game->player_count; ++i) {
            if (game->scores[i] > high) { high = game->scores[i]; leader = (int)i; tie = 0; }
            else if (game->scores[i] == high) tie = 1;
        }
        strncpy(game->winner, tie ? "DRAW" : game->players[leader], sizeof(game->winner) - 1);
    }
}

static const rx_fsm_transition transitions[] = {
    {RPS_STATE_WAITING_PLAYERS, RPS_STATE_WAITING_PLAYERS, guard_player_joined, action_join},
    {RPS_STATE_WAITING_PLAYERS, RPS_STATE_CONFIGURING_GAME, guard_enough_players, action_enter_config},
    {RPS_STATE_CONFIGURING_GAME, RPS_STATE_ROUND_OPEN, guard_config, action_enter_round_open},
    {RPS_STATE_ROUND_OPEN, RPS_STATE_WAITING_COMMITS, guard_start, action_enter_commits},
    {RPS_STATE_WAITING_COMMITS, RPS_STATE_WAITING_COMMITS, guard_commit, action_commit},
    {RPS_STATE_WAITING_COMMITS, RPS_STATE_WAITING_REVEALS, guard_all_commits, action_enter_reveals},
    {RPS_STATE_WAITING_COMMITS, RPS_STATE_WAITING_REVEALS, guard_commit_timeout, action_enter_reveals},
    {RPS_STATE_WAITING_REVEALS, RPS_STATE_RESOLVING_ROUND, guard_all_reveals, NULL},
    {RPS_STATE_WAITING_REVEALS, RPS_STATE_RESOLVING_ROUND, guard_reveal_timeout, NULL},
    {RPS_STATE_RESOLVING_ROUND, RPS_STATE_ROUND_FINISHED, guard_result, action_resolve},
    {RPS_STATE_ROUND_FINISHED, RPS_STATE_ROUND_OPEN, guard_next_round, action_enter_round_open},
    {RPS_STATE_ROUND_FINISHED, RPS_STATE_GAME_OVER, guard_winner, NULL}
};

void rps_game_init(rps_game_t *game) {
    memset(game, 0, sizeof(*game));
    game->config = rps_game_default_config();
    rx_fsm_runtime_init(&game->runtime, 1);
    rx_fsm_machine_init(&game->machine, "rps_game", RPS_STATE_WAITING_PLAYERS,
                        transitions, ARRAY_LEN(transitions), game, latch_inputs, dump_outputs);
    rx_fsm_runtime_add_machine(&game->runtime, &game->machine, 0, 0);
}

rps_state_t rps_game_state(const rps_game_t *game) {
    return (rps_state_t)game->machine.state;
}

int rps_game_add_player(rps_game_t *game, const char *player_id) {
    if (rps_game_state(game) != RPS_STATE_WAITING_PLAYERS) return -1;
    if (player_index(game, player_id) >= 0) return 0;
    strncpy(game->inputs.player_id, player_id, RPS_MAX_ID - 1);
    game->inputs.has_joined_player = 1;
    rps_game_tick(game, 1);
    return 0;
}

int rps_game_confirm_players(rps_game_t *game) {
    game->inputs.confirm_players = 1;
    rps_game_tick(game, 1);
    return rps_game_state(game) == RPS_STATE_CONFIGURING_GAME ? 0 : -1;
}

int rps_game_configure(rps_game_t *game, const rps_game_config_t *config) {
    if (rps_game_state(game) != RPS_STATE_CONFIGURING_GAME) return -1;
    if (config->target_score <= 0 || config->max_rounds <= 0) return -1;
    game->config = *config;
    game->inputs.config_committed = 1;
    rps_game_tick(game, 1);
    return 0;
}

int rps_game_start_round(rps_game_t *game) {
    if (rps_game_state(game) == RPS_STATE_WAITING_COMMITS) return 0;
    if (rps_game_state(game) != RPS_STATE_ROUND_OPEN) return -1;
    game->inputs.round_started = 1;
    rps_game_tick(game, 1);
    return 0;
}

int rps_game_submit_commit(rps_game_t *game, const char *player_id, const char *commit_hash) {
    int existing;
    if (rps_game_state(game) != RPS_STATE_WAITING_COMMITS) return -1;
    existing = commit_index(game, player_id);
    if (existing >= 0) {
        if (strcmp(game->commits[existing].hash, commit_hash) == 0) return 0;
        set_error(game, "conflicting commit: ", player_id);
        return -1;
    }
    strncpy(game->inputs.player_id, player_id, RPS_MAX_ID - 1);
    strncpy(game->inputs.commit_hash, commit_hash, RPS_HASH_HEX - 1);
    game->inputs.has_commit = 1;
    rps_game_tick(game, 1);
    return game->last_error[0] == '\0' || strstr(game->last_error, player_id) == NULL ? 0 : -1;
}

int rps_game_reveal(rps_game_t *game, const char *player_id, rps_move_t move, const char *nonce) {
    char expected[RPS_HASH_HEX];
    int cidx;
    int ridx;
    if (rps_game_state(game) != RPS_STATE_WAITING_REVEALS) return -1;
    cidx = commit_index(game, player_id);
    if (cidx < 0) {
        set_error(game, "reveal without commit: ", player_id);
        return -1;
    }
    rps_make_commitment(player_id, move, nonce, expected, sizeof(expected));
    if (strcmp(expected, game->commits[cidx].hash) != 0) {
        set_error(game, "invalid reveal: ", player_id);
        return -1;
    }
    ridx = reveal_index(game, player_id);
    if (ridx >= 0) {
        if (game->reveals[ridx].move == move) return 0;
        set_error(game, "conflicting reveal: ", player_id);
        return -1;
    }
    if (game->reveal_count < RPS_MAX_PLAYERS) {
        strncpy(game->reveals[game->reveal_count].player_id, player_id, RPS_MAX_ID - 1);
        game->reveals[game->reveal_count].move = move;
        game->reveal_count++;
    }
    snprintf(game->last_announcement, sizeof(game->last_announcement),
             "reveal received: %s -> %s", player_id, rps_move_name(move));
    rps_game_tick(game, 1);
    return 0;
}

void rps_game_tick(rps_game_t *game, int count) {
    int i;
    for (i = 0; i < count; ++i) rx_fsm_tick(&game->runtime);
}

int rps_game_score(const rps_game_t *game, const char *player_id) {
    int idx = player_index(game, player_id);
    return idx >= 0 ? game->scores[idx] : 0;
}
