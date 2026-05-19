/*
 * Modelo Promela/LTL para rps_c.
 *
 * Objetivo docente:
 *   - Verificar propiedades del modelo de juego, no de la implementacion TCP.
 *   - Mantener un estado finito pequeno: dos jugadores, puntuacion objetivo 1.
 *   - Separar decisiones sincronizadas por Raft de transiciones locales
 *     derivadas de la FSM.
 *
 * Uso basico con SPIN:
 *
 *   spin -a docs/ltl.pml
 *   cc -o pan pan.c
 *   ./pan -a -N p_players_bounded
 *
 * Para verificar otra propiedad, cambia el nombre tras -N.
 */

mtype = {
    WAITING_PLAYERS,
    CONFIGURING_GAME,
    ROUND_OPEN,
    WAITING_COMMITS,
    WAITING_REVEALS,
    RESOLVING_ROUND,
    ROUND_FINISHED,
    GAME_OVER
};

#define TARGET_SCORE 1
#define MAX_ROUNDS   3

mtype state = WAITING_PLAYERS;

bool joined0 = false;
bool joined1 = false;
bool commit0 = false;
bool commit1 = false;
bool reveal0 = false;
bool reveal1 = false;
bool winner0 = false;
bool winner1 = false;
bool draw_game = false;
bool error = false;

byte players = 0;
byte commits = 0;
byte reveals = 0;
byte score0 = 0;
byte score1 = 0;
byte round_no = 0;

#define enough_players        (joined0 && joined1)
#define all_committed         (commit0 && commit1)
#define all_revealed          ((!commit0 || reveal0) && (!commit1 || reveal1))
#define has_winner            (winner0 || winner1 || draw_game)
#define scores_bounded        ((score0 <= TARGET_SCORE) && (score1 <= TARGET_SCORE))
#define counts_bounded        ((players <= 2) && (commits <= players) && (reveals <= commits))
#define commits_have_players  ((!commit0 || joined0) && (!commit1 || joined1))
#define reveals_have_commits  ((!reveal0 || commit0) && (!reveal1 || commit1))

inline clear_round() {
    commit0 = false;
    commit1 = false;
    reveal0 = false;
    reveal1 = false;
    commits = 0;
    reveals = 0
}

inline resolve_round() {
    /*
     * Modelo abstracto del resultado.
     *
     * La implementacion C calcula el ganador usando ROCK/PAPER/SCISSORS y
     * commit-reveal. Aqui solo modelamos tres salidas posibles para mantener
     * finito el modelo: gana p0, gana p1 o empate.
     */
    if
    :: score0 < TARGET_SCORE -> score0++; winner0 = true
    :: score1 < TARGET_SCORE -> score1++; winner1 = true
    :: draw_game = (round_no >= MAX_ROUNDS)
    fi
}

proctype Game() {
    do
    :: atomic { state == WAITING_PLAYERS && !joined0 ->
        joined0 = true; players++ }
    :: atomic { state == WAITING_PLAYERS && !joined1 ->
        joined1 = true; players++ }
    :: atomic { state == WAITING_PLAYERS && enough_players ->
        state = CONFIGURING_GAME }

    :: atomic { state == CONFIGURING_GAME ->
        state = ROUND_OPEN }

    :: atomic { state == ROUND_OPEN && !has_winner ->
        clear_round(); round_no++; state = WAITING_COMMITS }

    :: atomic { state == WAITING_COMMITS && joined0 && !commit0 ->
        commit0 = true; commits++ }
    :: atomic { state == WAITING_COMMITS && joined1 && !commit1 ->
        commit1 = true; commits++ }
    :: atomic { state == WAITING_COMMITS && commit0 && !commit1 ->
        skip /* commit duplicado de p0: no-op */ }
    :: atomic { state == WAITING_COMMITS && commit1 && !commit0 ->
        skip /* commit duplicado de p1: no-op */ }
    :: atomic { state == WAITING_COMMITS && all_committed ->
        state = WAITING_REVEALS }

    :: atomic { state == WAITING_REVEALS && commit0 && !reveal0 ->
        reveal0 = true; reveals++ }
    :: atomic { state == WAITING_REVEALS && commit1 && !reveal1 ->
        reveal1 = true; reveals++ }
    :: atomic { state == WAITING_REVEALS && reveal0 && !reveal1 ->
        skip /* reveal duplicado de p0: no-op */ }
    :: atomic { state == WAITING_REVEALS && reveal1 && !reveal0 ->
        skip /* reveal duplicado de p1: no-op */ }
    :: atomic { state == WAITING_REVEALS && all_revealed ->
        state = RESOLVING_ROUND }

    :: atomic { state == RESOLVING_ROUND ->
        resolve_round(); state = ROUND_FINISHED }

    :: atomic { state == ROUND_FINISHED && has_winner ->
        state = GAME_OVER }
    :: atomic { state == ROUND_FINISHED && !has_winner && round_no < MAX_ROUNDS ->
        state = ROUND_OPEN }
    :: atomic { state == ROUND_FINISHED && !has_winner && round_no >= MAX_ROUNDS ->
        draw_game = true; state = GAME_OVER }

    :: atomic { state == GAME_OVER ->
        skip }
    od
}

init {
    run Game()
}

/*
 * PROPIEDADES LTL
 *
 * p_players_bounded:
 *   Nunca hay mas jugadores que el limite modelado.
 */
ltl p_players_bounded {
    [] (players <= 2)
}

/*
 * p_counts_bounded:
 *   Los contadores de commits y reveals nunca superan lo que pueden
 *   representar: no hay mas commits que jugadores ni mas reveals que commits.
 */
ltl p_counts_bounded {
    [] counts_bounded
}

/*
 * p_commit_requires_player:
 *   Nadie puede tener commit registrado si no era jugador de la partida.
 */
ltl p_commit_requires_player {
    [] commits_have_players
}

/*
 * p_reveal_requires_commit:
 *   Nadie puede revelar una jugada sin haber hecho commit previamente.
 */
ltl p_reveal_requires_commit {
    [] reveals_have_commits
}

/*
 * p_scores_bounded:
 *   Ninguna puntuacion supera la puntuacion objetivo del modelo.
 */
ltl p_scores_bounded {
    [] scores_bounded
}

/*
 * p_game_over_stable:
 *   GAME_OVER es terminal. Una vez alcanzado, no se vuelve a otro estado.
 */
ltl p_game_over_stable {
    [] (state == GAME_OVER -> [] (state == GAME_OVER))
}

/*
 * p_game_over_has_result:
 *   Si la partida termina, existe un resultado visible: gana p0, gana p1 o
 *   empate.
 */
ltl p_game_over_has_result {
    [] (state == GAME_OVER -> has_winner)
}

/*
 * p_no_error:
 *   El modelo de operaciones idempotentes no introduce errores por duplicados
 *   exactos. Los duplicados son no-op.
 */
ltl p_no_error {
    [] (!error)
}

/*
 * p_commits_progress:
 *   Cuando todos los jugadores han hecho commit, la FSM acaba pasando a la
 *   fase de reveals.
 */
ltl p_commits_progress {
    [] ((state == WAITING_COMMITS && all_committed) -> <> (state == WAITING_REVEALS))
}

/*
 * p_reveals_progress:
 *   Cuando todos los commits han sido revelados, la FSM acaba resolviendo y
 *   terminando la ronda.
 */
ltl p_reveals_progress {
    [] ((state == WAITING_REVEALS && all_revealed) -> <> (state == ROUND_FINISHED || state == GAME_OVER))
}

/*
 * p_eventual_terminal_after_winner:
 *   En cuanto existe ganador o empate global, la partida acaba alcanzando
 *   GAME_OVER.
 */
ltl p_eventual_terminal_after_winner {
    [] (has_winner -> <> (state == GAME_OVER))
}
