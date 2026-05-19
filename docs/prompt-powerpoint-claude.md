# Prompt autocontenido para Claude: presentación PowerPoint

Crea una presentación PowerPoint para alumnos de **Sistemas Empotrados
Avanzados** sobre cómo transformar una aplicación local basada en máquinas de
estados en una aplicación distribuida tolerante a fallos usando Raft.

No tienes acceso a ningún repositorio externo. Todo el contexto que necesitas
está incluido en este prompt. No inventes APIs ni dependencias adicionales.

## Audiencia

Alumnos de máster o últimos cursos de ingeniería con conocimientos de:

- programación en C;
- sistemas empotrados;
- máquinas de estados;
- concurrencia básica;
- nociones iniciales de sistemas distribuidos.

El objetivo es enseñar una **metodología de ingeniería**, no solo una demo.

## Tono y estilo

- Técnico, didáctico y riguroso.
- Nada de marketing.
- Explicaciones claras, con diagramas y snippets breves.
- Cada diapositiva debe tener poco texto visible y buenas notas del ponente.
- Prioriza comprensión conceptual y trazabilidad entre especificación, modelo,
  implementación y distribución.

## Formato de salida que quiero

Genera una especificación de presentación PowerPoint, no código.

Para cada diapositiva incluye:

1. Número y título.
2. Objetivo de aprendizaje.
3. Layout recomendado.
4. Contenido visible en la diapositiva.
5. Visual sugerido.
6. Notas del ponente.

La presentación debe tener **28 diapositivas**.

## Tema de la presentación

Usaremos como hilo conductor un juego distribuido de piedra-papel-tijera
implementado en C.

La aplicación empieza como una máquina de estados local y termina como una
aplicación replicada con Raft:

```text
especificación LTL
  -> modelo FSM
  -> implementación C local
  -> comandos replicados con Raft
  -> aplicación tolerante a fallos
```

El mensaje central:

> Raft no sustituye al diseño de la aplicación. Raft ordena entradas. La
> aplicación sigue necesitando una FSM determinista, propiedades verificables e
> idempotencia.

## Sistema de ejemplo

Juego piedra-papel-tijera para varios procesos. Cada proceso ejecuta la misma
aplicación. Los jugadores envían commits y luego reveals.

Estados de la FSM:

```text
WAITING_PLAYERS
CONFIGURING_GAME
ROUND_OPEN
WAITING_COMMITS
WAITING_REVEALS
RESOLVING_ROUND
ROUND_FINISHED
GAME_OVER
```

Movimientos:

```text
ROCK
PAPER
SCISSORS
```

Configuración:

```c
typedef struct {
    int target_score;
    int max_rounds;
    int commit_timeout_ticks; /* -1 = infinito/local */
    int reveal_timeout_ticks; /* -1 = infinito/local */
} rps_game_config_t;
```

Estados en C:

```c
typedef enum {
    RPS_STATE_WAITING_PLAYERS = 0,
    RPS_STATE_CONFIGURING_GAME = 1,
    RPS_STATE_ROUND_OPEN = 2,
    RPS_STATE_WAITING_COMMITS = 3,
    RPS_STATE_WAITING_REVEALS = 4,
    RPS_STATE_RESOLVING_ROUND = 5,
    RPS_STATE_ROUND_FINISHED = 6,
    RPS_STATE_GAME_OVER = 7
} rps_state_t;
```

Operaciones externas sincronizadas por Raft:

```text
player_joined(player_id)
confirm_players()
configure(config)
start_round()
commit_move(player_id, commit_hash)
reveal_move(player_id, move, nonce)
```

Operación auxiliar de infraestructura:

```text
peer_set(node_id, address)
```

No existe operación `tick` en el log Raft. Las transiciones automáticas se
drenan localmente después de aplicar operaciones comprometidas.

## Modelo de transiciones

Usa estas transiciones en la presentación. Deben aparecer como tabla o como
diagrama de estados:

```text
WAITING_PLAYERS -- player_joined(p) [p no existe] / add_player(p) --> WAITING_PLAYERS
WAITING_PLAYERS -- player_joined(p) [p ya existe] / no-op --> WAITING_PLAYERS
WAITING_PLAYERS -- confirm_players [players >= 2] / init_scores --> CONFIGURING_GAME
CONFIGURING_GAME -- configure(config) [config valida] / set_config, clear_round --> ROUND_OPEN
ROUND_OPEN -- start_round / round_number++, clear_round, set_commit_deadline --> WAITING_COMMITS
WAITING_COMMITS -- start_round [ronda ya abierta] / no-op --> WAITING_COMMITS
WAITING_COMMITS -- commit_move(p,h) [p existe, sin commit] / commits[p]=h --> WAITING_COMMITS
WAITING_COMMITS -- commit_move(p,h) [commits[p]==h] / no-op --> WAITING_COMMITS
WAITING_COMMITS -- commit_move(p,h2) [commits[p]!=h2] / error(conflicting commit) --> WAITING_COMMITS
WAITING_COMMITS -- all_commits_received / set_reveal_deadline --> WAITING_REVEALS
WAITING_COMMITS -- commit_timeout_local / set_reveal_deadline --> WAITING_REVEALS
WAITING_REVEALS -- reveal_move(p,m,n) [hash(p,m,n)==commits[p], sin reveal] / reveals[p]=m --> WAITING_REVEALS
WAITING_REVEALS -- reveal_move(p,m,n) [mismo reveal valido] / no-op --> WAITING_REVEALS
WAITING_REVEALS -- reveal_move(p,m,n) [hash invalido] / error(invalid reveal) --> WAITING_REVEALS
WAITING_REVEALS -- all_reveals_received / no-op --> RESOLVING_ROUND
WAITING_REVEALS -- reveal_timeout_local / no-op --> RESOLVING_ROUND
RESOLVING_ROUND -- result_committed / resolve_round, update_score --> ROUND_FINISHED
ROUND_FINISHED -- winner_exists / no-op --> GAME_OVER
ROUND_FINISHED -- next_round_needed / clear_round --> ROUND_OPEN
GAME_OVER -- any / no-op --> GAME_OVER
```

## Propiedades LTL que debe explicar la presentación

Incluye una sección de especificación formal con estas propiedades. Explica qué
significa cada una y qué bug detectaría.

### P1. Jugadores acotados

```text
[] (players <= 2)
```

Nunca hay más jugadores que el límite del modelo.

### P2. Contadores coherentes

```text
[] (players <= 2 && commits <= players && reveals <= commits)
```

No hay más commits que jugadores ni más reveals que commits.

### P3. Commit requiere jugador

```text
[] ((commit0 -> joined0) && (commit1 -> joined1))
```

Nadie puede tener commit si no era jugador.

### P4. Reveal requiere commit

```text
[] ((reveal0 -> commit0) && (reveal1 -> commit1))
```

Nadie puede revelar sin commit previo.

### P5. Puntuación acotada

```text
[] (score0 <= TARGET_SCORE && score1 <= TARGET_SCORE)
```

La puntuación no supera el objetivo.

### P6. GAME_OVER terminal

```text
[] (state == GAME_OVER -> [] (state == GAME_OVER))
```

Una vez terminada la partida, no se vuelve a otro estado.

### P7. GAME_OVER implica resultado

```text
[] (state == GAME_OVER -> (winner0 || winner1 || draw_game))
```

Si la partida termina, hay ganador o empate.

### P8. Duplicados exactos no producen error

```text
[] (!error)
```

En el modelo de duplicados exactos, repetir una operación idéntica es no-op.

### P9. Progreso de commits

```text
[] ((state == WAITING_COMMITS && all_committed) -> <> (state == WAITING_REVEALS))
```

Cuando todos han hecho commit, se llega a la fase de reveals.

### P10. Progreso de reveals

```text
[] ((state == WAITING_REVEALS && all_revealed) -> <> (state == ROUND_FINISHED || state == GAME_OVER))
```

Cuando todos revelan, se resuelve la ronda.

### P11. Resultado final alcanza terminal

```text
[] (has_winner -> <> (state == GAME_OVER))
```

Si ya hay ganador o empate, la partida acaba alcanzando `GAME_OVER`.

## Promela conceptual para mostrar

Incluye un snippet pequeño, no todo el modelo:

```promela
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

#define all_committed (commit0 && commit1)
#define all_revealed  ((!commit0 || reveal0) && (!commit1 || reveal1))

ltl p_reveal_requires_commit {
    [] ((!reveal0 || commit0) && (!reveal1 || commit1))
}
```

Incluye también el comando:

```bash
spin -a docs/ltl.pml
cc -o pan pan.c
./pan -a -N p_reveal_requires_commit
```

## Implementación local en C

Presenta las funciones principales de la FSM local:

```c
int rps_game_add_player(rps_game_t *game, const char *player_id);
int rps_game_confirm_players(rps_game_t *game);
int rps_game_configure(rps_game_t *game, const rps_game_config_t *config);
int rps_game_start_round(rps_game_t *game);
int rps_game_submit_commit(rps_game_t *game,
                           const char *player_id,
                           const char *commit_hash);
int rps_game_reveal(rps_game_t *game,
                    const char *player_id,
                    rps_move_t move,
                    const char *nonce);
```

Explica:

- la FSM local no conoce Raft;
- la FSM local no conoce TCP;
- la FSM local es determinista;
- las acciones duplicadas exactas son no-op;
- los conflictos sí son errores.

## Commit-reveal

Explica el mecanismo:

```text
commit_hash = SHA256(player_id ":" move ":" nonce)
```

Ejemplo:

```text
alice:0:nonce-a
```

Primero se replica:

```text
commit_move("alice", hash)
```

Después se replica:

```text
reveal_move("alice", ROCK, "nonce-a")
```

La FSM recalcula el hash. Si no coincide, el reveal es inválido y no modifica
el estado.

## Paso a Raft

Explica esta regla central:

> Una acción de usuario no existe para la FSM hasta que está comprometida en el
> log de Raft.

Flujo:

```text
usuario/CLI
  -> rps_operation_t
  -> raft_command_t
  -> líder Raft
  -> log replicado
  -> entrada comprometida
  -> raft_application_t.apply()
  -> rps_game_t
```

Comando Raft conceptual:

```c
typedef struct {
    char op[32];
    char key[64];
    char value[128];
} raft_command_t;
```

Ejemplo:

```text
op    = "rps.commit_move"
key   = "00000007"
value = "alice,<hash>"
```

## Aplicación Raft

Explica esta interfaz:

```c
typedef struct {
    void   (*apply)(void *user, const raft_command_t *command);
    void   (*reload)(void *user);
    size_t (*snapshot)(void *user, void *buf, size_t buf_size);
    void   (*restore_snapshot)(void *user, const void *buf, size_t size);
    void  *user;
} raft_application_t;
```

Estado conceptual:

```c
typedef struct {
    rps_game_t game;
    applied_key_set applied_keys;
    operation_id_set applied_operation_ids;
    applied_command_log applied_commands;
} rps_app_t;
```

Responsabilidad de `apply()`:

1. descartar `command.key` ya aplicada;
2. decodificar `raft_command_t`;
3. detectar duplicados lógicos si hay `operation_id`;
4. llamar a la FSM;
5. drenar transiciones automáticas;
6. recordar el comando para snapshot/replay.

## Drenaje local de transiciones automáticas

Explica que no hay `tick` en Raft. Las transiciones derivadas se drenan localmente:

```c
static void drain_automatic_transitions(rps_game_t *game) {
    for (int i = 0; i < 8; ++i) {
        rps_state_t before = rps_game_state(game);
        rps_game_tick(game, 1);
        if (rps_game_state(game) == before) return;
    }
}
```

Mensaje clave:

> El log Raft contiene decisiones de juego, no pulsos de reloj.

## Idempotencia

Haz una sección específica.

Duplicado exacto:

```text
commit_move(alice, hashA)
commit_move(alice, hashA)
```

Resultado:

```text
segundo comando = no-op
sin error
sin duplicar contador
sin cambiar puntuación
```

Conflicto:

```text
commit_move(alice, hashA)
commit_move(alice, hashB)
```

Resultado:

```text
error(conflicting commit)
estado no mutado por el segundo
```

Explica que esto es esencial para:

- transportes que duplican mensajes;
- reintentos de cliente;
- replay de log tras reinicio;
- restore de snapshot.

## Snapshot y recuperación

Explica:

- Raft no puede guardar logs infinitos;
- un nodo reiniciado necesita reconstruir estado;
- `snapshot()` compacta el estado de aplicación;
- `restore_snapshot()` reconstruye la FSM;
- después se reaplica el sufijo del log.

Para este ejemplo, snapshot didáctico:

```text
rps.player_joined    00000001    alice
rps.player_joined    00000002    bob
rps.confirm_players  00000003
rps.configure        00000004    1,9,-1,-1
```

Explica que en producción podría serializarse el estado visible directamente,
pero sin olvidar metadatos de idempotencia.

## CLI y nodos reales

Explica que la CLI también se modela como FSM:

- lee entrada humana;
- convierte comandos a operaciones;
- si es líder, propone a Raft;
- si es follower, reenvía al líder;
- nunca muta directamente `rps_game_t`.

Ejemplo de cluster:

```bash
./build/rps_node --id n1 --player alice --port 9101 \
  --member n1 --member n2 --member n3 \
  --peer n2=127.0.0.1:9102 --peer n3=127.0.0.1:9103
```

Comandos CLI:

```text
status
join PLAYER
confirm
config [target_score] [max_rounds] [commit_timeout] [reveal_timeout]
start
commit piedra|papel|tijera [nonce]
reveal piedra|papel|tijera NONCE
score
log
help
quit
```

## Errores comunes y cómo evitarlos

Incluye una sección con tabla de tres columnas:

```text
Error | Síntoma | Prevención
```

Errores:

1. Aplicar localmente antes del commit.
2. Meter lógica de dominio dentro de `apply()`.
3. Tratar duplicados exactos como error.
4. No distinguir duplicado de conflicto.
5. Sincronizar ticks/reloj cuando no hace falta.
6. Serializar comandos sin límites.
7. Hacer snapshots incompletos.
8. Diseñar la CLI como bucle imperativo que muta estado.
9. Mezclar descubrimiento de peers con reglas del juego.
10. Empezar por sockets antes de tener propiedades y FSM.

## Estructura exacta de 28 diapositivas

Usa esta estructura:

1. **Título**  
   Visual: diagrama simple FSM -> Raft -> nodos replicados.  
   Mensaje: de FSM local a sistema distribuido tolerante a fallos.

2. **Problema de partida**  
   Visual: dos nodos reciben eventos en órdenes distintos.  
   Explica por qué propagar mensajes no basta.

3. **Metodología de la asignatura**  
   Visual: pipeline LTL -> FSM -> C -> Raft -> validación.  
   Explica qué se produce en cada fase.

4. **Sistema de ejemplo**  
   Visual: piedra-papel-tijera con dos o tres procesos.  
   Introduce commit-reveal y estados.

5. **Qué verificamos antes de programar**  
   Visual: lista agrupada de propiedades.  
   Introduce safety vs liveness.

6. **LTL: invariantes de seguridad**  
   Visual: tabla P1-P5.  
   Incluye jugadores acotados, contadores, commit/reveal, score.

7. **LTL: progreso y terminalidad**  
   Visual: tabla P6-P11.  
   Incluye GAME_OVER terminal y progreso de commits/reveals.

8. **Promela mínimo**  
   Visual: snippet de `mtype`, macros y una propiedad.  
   Explica cómo usar SPIN.

9. **Estados de la FSM**  
   Visual: diagrama de estados con los 8 estados.  
   Explica responsabilidad de cada estado.

10. **Transiciones una por línea**  
    Visual: tabla evento/guarda/acción/destino.  
    Enseña la notación `ESTADO -- evento [guarda] / acción --> ESTADO`.

11. **Idempotencia en el modelo**  
    Visual: duplicado exacto vs conflicto.  
    Explica no-op para duplicados y error para conflictos.

12. **Commit-reveal**  
    Visual: línea temporal commit -> reveal -> validación.  
    Explica SHA-256 y nonce.

13. **Implementación C local**  
    Visual: `rps_state_t` y funciones `rps_game_*`.  
    Explica que la FSM no conoce red ni Raft.

14. **Guardas y acciones**  
    Visual: ejemplo de commit y reveal.  
    Explica cómo las guardas protegen invariantes.

15. **Tests del modelo local**  
    Visual: pirámide de tests.  
    Explica happy path, reveal inválido, duplicados.

16. **Por qué necesitamos Raft**  
    Visual: log ordenado replicado.  
    Explica consenso como orden total de entradas.

17. **Regla de oro**  
    Visual: "no apply before commit".  
    Explica que la FSM solo cambia desde entradas comprometidas.

18. **Operaciones sincronizadas**  
    Visual: lista de operaciones `rps.*`.  
    Aclara que no hay `tick` en el log.

19. **Codificación como `raft_command_t`**  
    Visual: campos op/key/value.  
    Muestra ejemplo `rps.commit_move`.

20. **`raft_application_t`**  
    Visual: interfaz de callbacks.  
    Explica `apply`, `reload`, `snapshot`, `restore_snapshot`.

21. **Adaptador RPS sobre Raft**  
    Visual: `rps_app_t` como puente.  
    Explica decode, idempotencia, llamada a FSM.

22. **Drenaje de transiciones automáticas**  
    Visual: committed command -> apply -> drain -> stable state.  
    Explica por qué no sincronizamos ticks.

23. **Snapshot y recuperación**  
    Visual: snapshot + sufijo de log.  
    Explica compactación y replay.

24. **CLI como FSM**  
    Visual: stdin -> operación -> líder/follower.  
    Explica que la CLI no muta estado.

25. **Cluster real**  
    Visual: tres nodos con líder y followers.  
    Incluye comando de ejemplo.

26. **Errores comunes I**  
    Visual: tabla de errores 1-5.  
    Enfatiza apply-before-commit, duplicados, ticks innecesarios.

27. **Errores comunes II**  
    Visual: tabla de errores 6-10.  
    Enfatiza snapshots, límites de C, CLI, peers.

28. **Resumen final**  
    Visual: pipeline completo con checks.  
    Mensaje final: propiedades -> FSM -> C -> Raft -> tolerancia a fallos.

## Reglas de diseño para las diapositivas

- Usa diagramas de flujo y tablas compactas.
- No pongas bloques de código de más de 12 líneas.
- Evita párrafos largos en las diapositivas; usa notas del ponente para detalle.
- Cada sección debe conectar con la anterior.
- Repite la regla central varias veces: la FSM solo consume entradas
  comprometidas.
- Destaca explícitamente la diferencia entre:
  - duplicado exacto;
  - conflicto;
  - replay;
  - timeout local;
  - decisión sincronizada.

Ahora genera la especificación completa de las 28 diapositivas siguiendo este
prompt.
