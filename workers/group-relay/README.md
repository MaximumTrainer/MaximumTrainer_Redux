# MaximumTrainer group-workout relay

A Cloudflare Worker with one Durable Object per room. Riders connect over a
WebSocket and the room passes small JSON messages between them: the roster,
the host's workout (the `.workout` XML), a shared start time, and 1 Hz live
numbers. Trainer control never depends on it.

Nothing about a ride is kept. A room stores only what a reconnecting rider
needs (roster tokens, the workout, the start time) and deletes it 30 minutes
after the last rider leaves.

## Protocol (v1)

Connect to `/ws?v=1&create=1&name=…` to host, or `/ws?v=1&code=7KQ2&name=…[&token=…]`
to join or rejoin. The relay answers with `welcome` (room code, your rider id,
rejoin token, server time, and the workout/start time if already set) or with
an `error` frame and a close (`room_not_found`, `room_full`, `protocol_mismatch`, …).

| Message   | From            | Fields                                  |
|-----------|-----------------|-----------------------------------------|
| `roster`  | relay           | `hostId`, `riders[{id,name,host,ready,sensors}]` |
| `workout` | host            | `name`, `xml` (≤ 64 KB)                 |
| `ready`   | rider           | `ready`, `sensors{power,hr,cad}`        |
| `ping`/`pong` | rider/relay | `t0` / `t0`, `serverMs`                 |
| `start`   | host            | `at` (server epoch ms, ≤ 5 min ahead)   |
| `tel`     | rider           | `t,p,tgt,hr,cad,iv,st`; relay adds `id` |
| `leave`   | rider           |                                         |

Limits: 20 riders per room, telemetry at most every 300 ms per rider, rooms
expire after 12 hours.

## Run locally

```bash
cd workers/group-relay
npx wrangler dev --port 8787          # no Cloudflare account needed
node test/relay.smoke.mjs              # relay smoke test
```

App side, against the local relay:

```bash
LD_LIBRARY_PATH=/tmp/qwt6/lib ./build/release/MaximumTrainer --group-sim 20 30
GROUP_RELAY_URL=ws://127.0.0.1:8787 ./build/tests/group_tests -v2
```

## Deploy

```bash
npx wrangler login     # once
npx wrangler deploy
curl https://mt-group-relay.<account-subdomain>.workers.dev/health
```

Free-plan usage: incoming WebSocket messages are billed at 20:1 and outgoing
ones are free, so a one-hour ride with 20 riders is about 3,600 requests of the
100,000 daily allowance. Room run time (about 28 room-hours a day) is the
tighter free-plan limit.
