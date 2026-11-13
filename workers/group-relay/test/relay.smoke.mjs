// Smoke test against a running relay:  node test/relay.smoke.mjs [ws://127.0.0.1:8787]
import assert from "node:assert/strict";

const base = process.argv[2] || "ws://127.0.0.1:8787";

class Client {
  constructor(params) {
    const query = new URLSearchParams({ v: "1", ...params });
    this.messages = [];
    this.waiters = [];
    this.ws = new WebSocket(`${base}/ws?${query}`);
    this.ws.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (process.env.DEBUG) console.log(params.name, event.data.slice(0, 120));
      this.messages.push(message);
      const waiter = this.waiters.find((candidate) => candidate.accepts(message));
      if (!waiter) return;
      this.waiters.splice(this.waiters.indexOf(waiter), 1);
      this.messages.splice(this.messages.indexOf(message), 1);
      waiter.resolve(message);
    };
    this.closed = new Promise((resolve) => (this.ws.onclose = (e) => resolve(e)));
    this.welcome = this.next((m) => m.type === "welcome");
    this.welcome.catch(() => {}); // clients that are refused never get one
  }
  next(accepts, timeoutMs = 3000) {
    const found = this.messages.find(accepts);
    if (found) { this.messages.splice(this.messages.indexOf(found), 1); return Promise.resolve(found); }
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("timeout waiting for message")), timeoutMs);
      this.waiters.push({ accepts, resolve: (message) => { clearTimeout(timer); resolve(message); } });
    });
  }
  send(message) { this.ws.send(JSON.stringify(message)); }
  close() { this.ws.close(1000); return this.closed; }
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const host = new Client({ create: "1", name: "Host" });
const hostWelcome = await host.welcome;
assert.equal(hostWelcome.host, true);
assert.match(hostWelcome.code, /^GRP-[0-9A-Z]{4}$/);
const code = hostWelcome.code;
console.log("created", code);

const rider = new Client({ code: code.toLowerCase(), name: "Julie" });
const riderWelcome = await rider.welcome;
assert.equal(riderWelcome.host, false);
const roster = await host.next((m) => m.type === "roster" && m.riders.length === 2);
assert.deepEqual(roster.riders.map((r) => r.name), ["Host", "Julie"]);

rider.send({ type: "workout", name: "x", xml: "<x/>" });
assert.equal((await rider.next((m) => m.type === "error")).code, "host_only");

host.send({ type: "workout", name: "Over-Unders", xml: "<workout/>" });
assert.equal((await rider.next((m) => m.type === "workout")).name, "Over-Unders");

rider.send({ type: "ready", ready: true, sensors: { power: true } });
const readyRoster = await host.next((m) => m.type === "roster" && m.riders.some((r) => r.ready));
assert.equal(readyRoster.riders.find((r) => r.name === "Julie").ready, true);

rider.send({ type: "ping", t0: 123 });
const pong = await rider.next((m) => m.type === "pong");
assert.equal(pong.t0, 123);
assert.ok(Math.abs(pong.serverMs - Date.now()) < 5000);

const at = Date.now() + 2000;
host.send({ type: "start", at });
assert.equal((await rider.next((m) => m.type === "start")).at, at);

host.send({ type: "tel", t: 1, p: 250, hr: 140 });
const tel = await rider.next((m) => m.type === "tel");
assert.equal(tel.id, hostWelcome.you);
assert.equal(tel.p, 250);
assert.equal(host.messages.some((m) => m.type === "tel"), false, "sender must not get its own telemetry");

// Rejoin with the token keeps the id and sees the stored workout and start time.
rider.ws.close(4000);
await rider.closed;
await sleep(200);
const again = new Client({ code, name: "Julie", token: riderWelcome.token });
const againWelcome = await again.welcome;
assert.equal(againWelcome.you, riderWelcome.you);
assert.equal(againWelcome.startAt, at);
assert.equal(againWelcome.workout.name, "Over-Unders");

const missing = new Client({ code: "GRP-ZZZZ", name: "Nobody" });
assert.equal((await missing.next((m) => m.type === "error")).code, "room_not_found");
await missing.closed;

// Fill the room up to the cap: host + again = 2, add 18 more, the 21st is refused.
const extras = [];
for (let i = 0; i < 18; ++i) extras.push(new Client({ code, name: `Sim ${i + 1}` }));
await Promise.all(extras.map((c) => c.welcome));
const overflow = new Client({ code, name: "Late" });
assert.equal((await overflow.next((m) => m.type === "error")).code, "room_full");
await overflow.closed;

// Host leaving hands the room to the next rider.
await host.close();
const handover = await again.next((m) => m.type === "roster" && m.hostId === againWelcome.you);
assert.equal(handover.riders.length, 19);

const badVersion = new Client({ code, name: "Old", v: "0" });
assert.equal((await badVersion.next((m) => m.type === "error")).code, "protocol_mismatch");

await Promise.all([again.close(), ...extras.map((c) => c.close())]);
console.log("relay smoke test passed");
