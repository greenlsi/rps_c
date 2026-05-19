# Modelo de máquinas de estados

Este documento describe el modelo docente de `rps_c` antes de introducir los
detalles de C, TCP y Raft. La intención es que cada transición pueda leerse como
una línea simple: estado origen, evento o guarda, estado destino y acción.

## Estados

| Estado | Significado |
| --- | --- |
| `WAITING_PLAYERS` | La partida acepta jugadores. |
| `CONFIGURING_GAME` | Ya hay jugadores suficientes y se acepta configuración. |
| `ROUND_OPEN` | La partida está lista para abrir una ronda. |
| `WAITING_COMMITS` | Los jugadores envían hashes de sus jugadas. |
| `WAITING_REVEALS` | Los jugadores revelan jugada y nonce. |
| `RESOLVING_ROUND` | La FSM calcula el resultado de la ronda. |
| `ROUND_FINISHED` | La ronda terminó; se decide si continuar o terminar. |
| `GAME_OVER` | Estado terminal. |

## Entradas externas sincronizadas por Raft

Estas son las entradas que forman la historia compartida de la partida:

| Operación | Efecto |
| --- | --- |
| `player_joined(player_id)` | Asegura que el jugador existe. Repetirlo es no-op. |
| `confirm_players()` | Confirma que ya se puede configurar la partida. |
| `configure(config)` | Fija puntuación objetivo, rondas máximas y timeouts locales. |
| `start_round()` | Abre una ronda. Repetirlo justo después es no-op. |
| `commit_move(player_id, commit_hash)` | Registra el hash de la jugada. Repetir el mismo hash es no-op. |
| `reveal_move(player_id, move, nonce)` | Valida el hash y revela la jugada. Repetir el mismo reveal es no-op. |

`peer_set(node_id, address)` es una operación auxiliar de infraestructura para
aprender direcciones de nodos; no pertenece al dominio del juego.

## Datos del modelo

- `players`: conjunto ordenado de jugadores.
- `config`: `target_score`, `max_rounds`, `commit_timeout_ticks`, `reveal_timeout_ticks`.
- `round_number`: ronda actual.
- `scores[player]`: puntuación por jugador.
- `commits[player]`: hash comprometido por jugador en la ronda.
- `reveals[player]`: jugada revelada por jugador en la ronda.
- `winner`: jugador ganador o `DRAW`.
- `last_error`: último error local observable.
- `last_announcement`: último anuncio local observable.

## Transiciones principales

Una transición por línea:

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

## Separación entre modelo local y modelo distribuido

La FSM anterior no conoce Raft. Raft entra en la arquitectura como mecanismo de
ordenación de entradas externas:

```text
CLI o cliente
  -> rps_operation_t
  -> raft_command_t
  -> log Raft comprometido
  -> rps_app.apply()
  -> transición de rps_game_t
```

Después de aplicar una operación comprometida, cada nodo drena localmente las
transiciones automáticas de la FSM. Por eso no existe operación `tick` en el
log Raft: el log contiene decisiones de juego, no pulsos de reloj.

## Invariantes del modelo

- Nunca hay más commits que jugadores.
- Nunca hay más reveals que commits.
- Un commit solo existe para un jugador registrado.
- Un reveal solo existe si hay commit previo.
- `GAME_OVER` es terminal.
- Un resultado final implica ganador o empate.
- Los duplicados exactos son no-op, no errores.
- Las operaciones conflictivas sí producen error local y no mutan el estado.

## Correspondencia con código C

| Modelo | Código |
| --- | --- |
| Estados | `rps_state_t` en `include/rps/rps_game.h` |
| Entradas | funciones `rps_game_*` |
| Operaciones sincronizadas | `rps_operation_t` en `include/rps/rps_command.h` |
| Codificación Raft | `rps_command_encode()` y `rps_command_decode()` |
| Aplicación de comandos comprometidos | `rps_app.c` |
| Drenaje de transiciones automáticas | `drain_automatic_transitions()` en `rps_app.c` |
