// MaximumTrainer group-workout relay.
//
// One Durable Object per room passes small JSON messages between riders. Nothing
// about a ride is kept once the room empties: the stored state is only what a
// reconnecting rider needs (roster tokens, the workout, the start time).

const PROTOCOL_VERSION = 1;
const MAX_RIDERS = 20;
const MAX_NAME_LENGTH = 24;
const MAX_WORKOUT_BYTES = 64 * 1024;
const MAX_MESSAGE_BYTES = MAX_WORKOUT_BYTES + 1024;
const MIN_TELEMETRY_INTERVAL_MS = 300;
const EMPTY_ROOM_TTL_MS = 30 * 60 * 1000;
const MAX_ROOM_AGE_MS = 12 * 60 * 60 * 1000;
const CODE_ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; // Crockford base32
const CODE_LENGTH = 4;
const CREATE_ATTEMPTS = 8;

function randomCode() {
  const bytes = new Uint8Array(CODE_LENGTH);
  crypto.getRandomValues(bytes);
  return Array.from(bytes, (b) => CODE_ALPHABET[b % CODE_ALPHABET.length]).join("");
}

function normalizeCode(raw) {
  if (!raw) return null;
  const code = raw.toUpperCase().replace(/^GRP-/, "")
    .replace(/O/g, "0").replace(/[IL]/g, "1").replace(/[^0-9A-Z]/g, "");
  if (code.length !== CODE_LENGTH) return null;
  for (const c of code) if (!CODE_ALPHABET.includes(c)) return null;
  return code;
}

function randomToken() {
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  return Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
}

function sanitizeName(raw) {
  const name = (raw || "").replace(/[\u0000-\u001f\u007f]/g, "").trim().slice(0, MAX_NAME_LENGTH);
  return name || "Rider";
}

// Refusals are sent over an accepted socket so clients can read the reason;
// browsers and QWebSocket hide the HTTP status of a failed upgrade.
function refuse(errorCode, extra = {}) {
  const [client, server] = Object.values(new WebSocketPair());
  server.accept();
  server.send(JSON.stringify({ type: "error", code: errorCode, ...extra }));
  server.close(4000, errorCode);
  return new Response(null, { status: 101, webSocket: client });
}

function json(status, body) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname === "/health") {
      return json(200, { ok: true, protocol: PROTOCOL_VERSION });
    }

    if (url.pathname !== "/ws") return json(404, { error: "not_found" });
    if (request.headers.get("Upgrade") !== "websocket") {
      return json(426, { error: "websocket_required" });
    }
    if (Number(url.searchParams.get("v")) !== PROTOCOL_VERSION) {
      return refuse("protocol_mismatch", { protocol: PROTOCOL_VERSION });
    }

    if (url.searchParams.get("create") === "1") {
      for (let attempt = 0; attempt < CREATE_ATTEMPTS; ++attempt) {
        const code = randomCode();
        const response = await forwardToRoom(env, request, code, true);
        if (response.status !== 409) return response;
      }
      return refuse("no_free_code");
    }

    const code = normalizeCode(url.searchParams.get("code"));
    if (!code) return refuse("bad_code");
    return forwardToRoom(env, request, code, false);
  },
};

function forwardToRoom(env, request, code, create) {
  const url = new URL(request.url);
  url.searchParams.set("code", code);
  url.searchParams.set("create", create ? "1" : "0");
  const stub = env.ROOMS.get(env.ROOMS.idFromName(code));
  return stub.fetch(new Request(url.toString(), request));
}

export class GroupRoom {
  constructor(ctx, env) {
    this.ctx = ctx;
    this.env = env;
    this.loaded = false;
    this.ctx.setWebSocketAutoResponse(
      new WebSocketRequestResponsePair('{"type":"keepalive"}', '{"type":"keepalive"}'));
  }

  async load() {
    if (this.loaded) return;
    const stored = await this.ctx.storage.get(["meta", "riders", "workout"]);
    this.meta = stored.get("meta") || null;
    this.riders = stored.get("riders") || {}; // riderId -> { token, name, joinedAt }
    this.workout = stored.get("workout") || null;
    this.loaded = true;
  }

  async saveMeta() { await this.ctx.storage.put("meta", this.meta); }
  async saveRiders() { await this.ctx.storage.put("riders", this.riders); }

  connectedSockets() {
    return this.ctx.getWebSockets().filter((ws) => ws.deserializeAttachment()?.riderId);
  }

  socketsFor(riderId) {
    return this.connectedSockets().filter((ws) => ws.deserializeAttachment().riderId === riderId);
  }

  connectedRiderIds() {
    return [...new Set(this.connectedSockets().map((ws) => ws.deserializeAttachment().riderId))];
  }

  async fetch(request) {
    await this.load();
    const url = new URL(request.url);
    const code = url.searchParams.get("code");
    const create = url.searchParams.get("create") === "1";
    const name = sanitizeName(url.searchParams.get("name"));
    const rejoinToken = url.searchParams.get("token") || "";
    const now = Date.now();

    if (this.meta && now - this.meta.createdAt > MAX_ROOM_AGE_MS) {
      await this.wipe();
    }

    if (create) {
      if (this.meta) return json(409, { error: "code_in_use" });
      this.meta = { code, createdAt: now, hostId: null, startAt: null, nextId: 1 };
      this.riders = {};
      this.workout = null;
    } else if (!this.meta) {
      return refuse("room_not_found");
    }

    let riderId = null;
    for (const [id, rider] of Object.entries(this.riders)) {
      if (rejoinToken && rider.token === rejoinToken) riderId = id;
    }

    if (!riderId) {
      if (this.connectedRiderIds().length >= MAX_RIDERS) {
        return refuse("room_full", { max: MAX_RIDERS });
      }
      riderId = "r" + this.meta.nextId++;
      this.riders[riderId] = { token: randomToken(), name, joinedAt: now };
    } else {
      this.riders[riderId].name = name;
      for (const old of this.socketsFor(riderId)) old.close(4001, "replaced");
    }

    if (!this.meta.hostId) this.meta.hostId = riderId;
    await this.saveMeta();
    await this.saveRiders();
    await this.ctx.storage.deleteAlarm();

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({ riderId, ready: false, sensors: {}, lastTelemetryAt: 0 });

    this.send(server, {
      type: "welcome",
      protocol: PROTOCOL_VERSION,
      code: "GRP-" + this.meta.code,
      you: riderId,
      token: this.riders[riderId].token,
      host: this.meta.hostId === riderId,
      serverMs: now,
      maxRiders: MAX_RIDERS,
      startAt: this.meta.startAt,
      workout: this.workout,
    });
    this.broadcastRoster();

    return new Response(null, { status: 101, webSocket: client });
  }

  roster() {
    const connected = new Map();
    for (const ws of this.connectedSockets()) {
      const att = ws.deserializeAttachment();
      connected.set(att.riderId, att);
    }
    return [...connected.entries()]
      .map(([id, att]) => ({
        id,
        name: this.riders[id]?.name || "Rider",
        host: id === this.meta.hostId,
        ready: !!att.ready,
        sensors: att.sensors || {},
        joinedAt: this.riders[id]?.joinedAt || 0,
      }))
      .sort((a, b) => a.joinedAt - b.joinedAt);
  }

  send(ws, message) {
    try { ws.send(JSON.stringify(message)); } catch (_) { /* socket already closing */ }
  }

  broadcast(message, exceptWs = null) {
    const text = JSON.stringify(message);
    for (const ws of this.connectedSockets()) {
      if (ws === exceptWs) continue;
      try { ws.send(text); } catch (_) { /* socket already closing */ }
    }
  }

  broadcastRoster() {
    this.broadcast({ type: "roster", hostId: this.meta.hostId, riders: this.roster() });
  }

  error(ws, code, detail) {
    this.send(ws, { type: "error", code, detail });
  }

  async webSocketMessage(ws, raw) {
    await this.load();
    if (!this.meta) { ws.close(4004, "room_closed"); return; }
    const size = typeof raw === "string" ? raw.length : raw.byteLength;
    if (size > MAX_MESSAGE_BYTES) { this.error(ws, "too_large"); return; }

    let message;
    try { message = JSON.parse(typeof raw === "string" ? raw : new TextDecoder().decode(raw)); }
    catch (_) { this.error(ws, "bad_json"); return; }

    const att = ws.deserializeAttachment();
    const isHost = att.riderId === this.meta.hostId;

    switch (message.type) {
      case "ping":
        this.send(ws, { type: "pong", t0: message.t0, serverMs: Date.now() });
        return;

      case "tel": {
        const now = Date.now();
        if (now - att.lastTelemetryAt < MIN_TELEMETRY_INTERVAL_MS) return;
        att.lastTelemetryAt = now;
        ws.serializeAttachment(att);
        const { type, ...fields } = message;
        this.broadcast({ ...fields, type: "tel", id: att.riderId }, ws);
        return;
      }

      case "ready":
        att.ready = !!message.ready;
        att.sensors = typeof message.sensors === "object" && message.sensors ? message.sensors : {};
        ws.serializeAttachment(att);
        this.broadcastRoster();
        return;

      case "workout": {
        if (!isHost) { this.error(ws, "host_only"); return; }
        if (this.meta.startAt) { this.error(ws, "already_started"); return; }
        if (typeof message.xml !== "string" || message.xml.length > MAX_WORKOUT_BYTES) {
          this.error(ws, "bad_workout"); return;
        }
        this.workout = { name: String(message.name || "").slice(0, 200), xml: message.xml };
        await this.ctx.storage.put("workout", this.workout);
        this.broadcast({ type: "workout", ...this.workout });
        return;
      }

      case "start": {
        if (!isHost) { this.error(ws, "host_only"); return; }
        if (!this.workout) { this.error(ws, "no_workout"); return; }
        if (this.meta.startAt) { this.error(ws, "already_started"); return; }
        const now = Date.now();
        const at = Number(message.at);
        if (!Number.isFinite(at) || at < now || at > now + 5 * 60 * 1000) {
          this.error(ws, "bad_start_time"); return;
        }
        this.meta.startAt = at;
        await this.saveMeta();
        this.broadcast({ type: "start", at });
        return;
      }

      case "leave":
        ws.close(1000, "left");
        await this.onSocketGone(ws, true);
        return;

      default:
        this.error(ws, "unknown_type", message.type);
    }
  }

  async webSocketClose(ws, code, reason) {
    try { ws.close(code === 1005 ? 1000 : code, reason); } catch (_) { /* already closed */ }
    await this.load();
    await this.onSocketGone(ws, code === 1000);
  }

  async webSocketError(ws) {
    await this.load();
    await this.onSocketGone(ws, false);
  }

  async onSocketGone(ws, leftOnPurpose) {
    if (!this.meta) return;
    const att = ws.deserializeAttachment();
    if (!att?.riderId) return;
    ws.serializeAttachment({});

    const stillConnected = this.socketsFor(att.riderId).length > 0;
    if (!stillConnected && leftOnPurpose) {
      delete this.riders[att.riderId];
      await this.saveRiders();
    }

    const remaining = this.roster();
    if (!stillConnected && this.meta.hostId === att.riderId && leftOnPurpose) {
      this.meta.hostId = remaining.length ? remaining[0].id : null;
      await this.saveMeta();
    }

    if (remaining.length === 0) {
      await this.ctx.storage.setAlarm(Date.now() + EMPTY_ROOM_TTL_MS);
    } else {
      this.broadcastRoster();
    }
  }

  async alarm() {
    await this.load();
    if (this.connectedSockets().length === 0) await this.wipe();
  }

  async wipe() {
    await this.ctx.storage.deleteAll();
    this.meta = null;
    this.riders = {};
    this.workout = null;
  }
}
