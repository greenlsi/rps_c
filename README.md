# rps_c

Versión C de `rps_python` con Raft real sobre TCP usando:

- `/Users/josem/src/consulting/raft_rx/c`
- `/Users/josem/src/consulting/rxnet/c`

La guía didáctica para transformar una aplicación FSM en una aplicación
distribuida tolerante a fallos con Raft está en
[`docs/transformar-fsm-a-raft-rx.md`](docs/transformar-fsm-a-raft-rx.md).

## Build

```bash
make
make test
```

El binario principal es:

```bash
build/rps_node
```

## Ejemplo de cluster

Terminal 1:

```bash
./build/rps_node --id n1 --player alice --port 9101 \
  --member n1 --member n2 --member n3 \
  --peer n2=127.0.0.1:9102 --peer n3=127.0.0.1:9103
```

Terminal 2:

```bash
./build/rps_node --id n2 --player bob --port 9102 \
  --member n1 --member n2 --member n3 \
  --peer n1=127.0.0.1:9101 --peer n3=127.0.0.1:9103
```

Terminal 3:

```bash
./build/rps_node --id n3 --player carol --port 9103 \
  --member n1 --member n2 --member n3 \
  --peer n1=127.0.0.1:9101 --peer n2=127.0.0.1:9102
```

## CLI

```text
status
join HOST:PORT
player PLAYER
join PLAYER    (compat: add player)
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

No hay operación Raft `tick`: el log contiene decisiones de juego. Cada nodo
avanza localmente las transiciones automáticas de su FSM después de aplicar
operaciones comprometidas.

Para unir un nodo ya arrancado a otro clúster, usa `join HOST:PORT` en la CLI
del nodo que entra. Para añadir un jugador a la partida, usa `player PLAYER`.
