# RPS C Raft Design

## Goal

Build a C version of `/Users/josem/src/rps_python` in `/Users/josem/src/rps_c` as a real TCP Raft application. The delivered binary is `build/rps_node`, an interactive node process that uses `/Users/josem/src/consulting/raft_rx/c` for consensus and `/Users/josem/src/consulting/rxnet/c` for the cooperative runtime and CLI FSM.

## Scope

The first version implements a real `raft_rx` node with `raft_tcp_transport`, not an in-memory teaching cluster. User commands are serialized as `rps.*` Raft commands. The RPS game state changes only inside the Raft application `apply()` callback after log commit.

The public CLI is intentionally small:

```text
status
join PLAYER
confirm
config [target_score] [max_rounds] [commit_timeout] [reveal_timeout]
start
commit piedra|papel|tijera [nonce]
reveal piedra|papel|tijera [nonce]
score
log
help
quit
```

There is no public `tick`, `leader`, or `members` command. `status` includes Raft role, leader, membership, peers, game state, players, round, score, and pending commit/reveal counts.

## Architecture

`rps_game` owns the deterministic game FSM and is driven by `rxnet/fsm.h`. `rps_command` maps CLI/application operations to `raft_command_t`. `rps_app` implements `raft_application_t` and applies committed commands to the game. `rps_cli` is an rxnet FSM that polls stdin and TCP join/forward queues, then either submits commands locally on the leader or forwards them to the known leader.

The process wiring follows `/Users/josem/src/consulting/raft_rx/c/examples/demo_app`: one `rx_fsm_runtime`, one `raft_cluster_t`, one TCP transport, one CLI machine, and one `rx_coop_exec`.

## Consistency

All player joins, configuration, round starts, commits, and reveals must be log operations. Automatic FSM transitions are drained locally after committed operations are applied. A node never mutates visible game state from a local CLI command directly. Followers forward CLI commands to the leader where possible.

## Persistence

Raft persists its log and metadata through `raft_rx`. The RPS application supports snapshot and restore with a compact line-oriented operation history so replay and compaction keep the game recoverable.

## Testing

The C tests cover command serialization, commitment hashing compatible with Python SHA-256, core game transitions, invalid reveal handling, and application replay through `raft_application_t`.
