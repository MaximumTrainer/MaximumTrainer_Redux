# Intervals.icu CORS Proxy — Cloudflare Worker

Forwards requests from the Maximum Trainer WASM app
(`https://maximumtrainer.github.io`) to `https://intervals.icu` with CORS
headers, because intervals.icu does not natively support cross-origin browser
requests (confirmed: `OPTIONS /oauth/token` → HTTP 405, no CORS headers).

All responses — including OAuth token exchange, general proxy, and error
paths — carry `Access-Control-Allow-Origin: *` so that any frontend
(WASM, VirtualRow, maximum_trainer, local dev) can read both success and
error bodies.

---

## Setup

### 1 — Create a free Cloudflare account

Sign up at <https://cloudflare.com> (no credit card required).

### 2 — Get your API token and Account ID

In the Cloudflare dashboard:
- **API Token**: My Profile → API Tokens → Create Token → "Edit Cloudflare Workers" template.
- **Account ID**: shown in the right-hand sidebar on the Workers & Pages overview page.

### 3 — Add GitHub Secrets / Variables

In your GitHub repo → Settings → Secrets and variables → Actions:

| Type | Name | Value |
|------|------|-------|
| Secret | `CLOUDFLARE_API_TOKEN` | your token from step 2 |
| Secret | `CLOUDFLARE_ACCOUNT_ID` | your account ID from step 2 |

### 4 — Provision the CLIENT_SECRETS KV namespace (multi-tenant, recommended)

The worker looks up each client's OAuth secret from a Cloudflare KV namespace
keyed by `client_id`.  This allows multiple applications (WASM, VirtualRow,
desktop) to share a single worker while each uses its own registered client.

```bash
cd workers/intervals-cors-proxy

# Create the production KV namespace
wrangler kv namespace create "CLIENT_SECRETS"
# Output:  [[kv_namespaces]]
#          binding = "CLIENT_SECRETS"
#          id = "<generated-id>"

# (Optional) Create a preview namespace for local development
wrangler kv namespace create "CLIENT_SECRETS" --preview
```

Paste each namespace ID into `wrangler.toml` (replace the commented template):

```toml
[[kv_namespaces]]
binding = "CLIENT_SECRETS"
id = "<your-generated-namespace-id>"
preview_id = "<your-generated-preview-namespace-id>"   # from the --preview command
```

Then populate the credentials for each OAuth client:

```bash
# Store the secret for each OAuth client (repeat for every client_id you register)
wrangler kv key put "<client_id>" "<client_secret>" --binding=CLIENT_SECRETS --config wrangler.toml
```

### 4b — Legacy single-tenant fallback (optional)

If the CLIENT_SECRETS KV namespace is not configured, the worker falls back
to `INTERVALS_CLIENT_ID` + `INTERVALS_CLIENT_SECRET` worker env vars.  This is kept for
backwards compatibility with existing deployments:

```bash
cd workers/intervals-cors-proxy
printf '259' | npx wrangler secret put INTERVALS_CLIENT_ID
npx wrangler secret put INTERVALS_CLIENT_SECRET
# paste the client_secret for intervals.icu OAuth client 259
```

The `deploy-intervals-proxy.yml` workflow handles both paths automatically
from the `INTERVALS_OAUTH_CLIENT_SECRET` repository secret:
- If the KV namespace is configured in `wrangler.toml` → writes `kv key put`
- Always writes the legacy `INTERVALS_CLIENT_SECRET` Worker secret as fallback

> ⚠️ Verify what is actually deployed (JSON body with client_id):
> ```bash
> curl -s -X POST https://<worker-url>/proxy/api/oauth/token \
>   -H 'Content-Type: application/json' \
>   -d '{"client_id":"259","code":"x","redirect_uri":"http://localhost:1/"}'
> ```
> should return an intervals.icu JSON error (e.g. *invalid code*), **not** an
> empty 404 or a `{"error":"unauthorized_client"}` 401.

### 5 — Deploy the worker

Either via GitHub Actions (automatic after setting the secrets — the
`deploy-intervals-proxy.yml` workflow deploys on every push to master that
touches `workers/intervals-cors-proxy/**`) **or** manually:

```bash
cd workers/intervals-cors-proxy
npx wrangler deploy --account-id <YOUR_ACCOUNT_ID>
```

### 6 — Note the worker URL

After deployment, Wrangler prints something like:

```
Published mt-intervals-proxy (0.12 sec)
  https://mt-intervals-proxy.<your-subdomain>.workers.dev
```

Copy that URL.

### 7 — Set `INTERVALS_PROXY_URL` in GitHub

Repo → Settings → Secrets and variables → Actions → Variables tab:

| Name | Value |
|------|-------|
| `INTERVALS_PROXY_URL` | `https://mt-intervals-proxy.<your-subdomain>.workers.dev` |

### 8 — Trigger a pages.yml re-deploy

Push any change to `docs/` or `master`, or manually trigger the
"Deploy GitHub Pages" workflow.  The workflow injects `INTERVALS_PROXY_URL`
into `docs/app/index.html` before deploying, enabling the WASM app to
proxy all `intervals.icu` requests through the Worker.

## OAuth token exchange

The dedicated token endpoint (`POST /proxy/api/oauth/token`) is open to any
caller — no `Origin` or `X-MT-Client` header required.  Callers must supply
`client_id` so the worker can look up the corresponding secret from the KV
store:

**JSON input (preferred):**
```json
{ "client_id": "259", "code": "<authorization_code>", "redirect_uri": "<redirect_uri>" }
```

**Form-encoded input (legacy / desktop client):**
```
grant_type=authorization_code&client_id=259&code=<code>&redirect_uri=<redirect_uri>
```

Intervals.icu has no refresh tokens — its token endpoint implements only the
authorization-code exchange, so `refresh_token` grants are rejected with
`unsupported_grant_type`.  Re-verified upstream on 2026-08-31 (#359): the
endpoint requires `code` whatever `grant_type` says, and behaves identically
when `grant_type` is omitted entirely, so it never reads a `refresh_token`:

```
grant_type=refresh_token&client_id=259&client_secret=…&refresh_token=…
→ 422 {"error":"Required request parameter 'code' … is not present"}

grant_type=refresh_token&client_id=259&client_secret=dummy&code=probe   → 404 "Client and/or secret not found"
(no grant_type)&client_id=259&client_secret=dummy&code=probe            → 404 "Client and/or secret not found"
```

Clients must therefore treat a `401` as "session expired — sign in again"
rather than attempting a refresh.

The worker looks up the `client_secret` from the `CLIENT_SECRETS` KV namespace
using the supplied `client_id` as the key, then appends `grant_type`,
`client_id`, and the retrieved `client_secret` before forwarding to
`https://intervals.icu/api/oauth/token`.

**Error responses (KV mode — `CLIENT_SECRETS` binding configured):**

| HTTP | `error` field | Cause |
|------|---------------|-------|
| 400 | `missing_client_id` | Request body contains no `client_id` |
| 401 | `unauthorized_client` | `client_id` not registered in KV |
| 400 | `unsupported_grant_type` | `grant_type` is not `authorization_code` |
| 503 | `kv_unavailable` | KV namespace is unreachable or timed out |
| 500 | `internal_error` | Unexpected exception |

**Error responses (legacy env-var mode — no KV binding):**

| HTTP | `error` field | Cause |
|------|---------------|-------|
| 500 | `worker_misconfigured` | `INTERVALS_CLIENT_ID` or `INTERVALS_CLIENT_SECRET` env var missing |
| 400 | `unsupported_grant_type` | `grant_type` is not `authorization_code` |
| 500 | `internal_error` | Unexpected exception |

---

## Request routing

```
Any frontend (WASM, VirtualRow, maximum_trainer, …)
  → https://mt-intervals-proxy.<subdomain>.workers.dev/proxy/api/oauth/token
  → Cloudflare Worker (KV secret lookup, builds form body, adds CORS *)
  → https://intervals.icu/api/oauth/token

WASM app (browser, general API calls)
  → https://mt-intervals-proxy.<subdomain>.workers.dev/proxy/<path>
  → Cloudflare Worker (adds CORS headers)
  → https://intervals.icu/<path>
```

The `index.html` fetch/XHR interceptor rewrites every request whose URL
starts with `https://intervals.icu` to the proxy before any network call
is made.

---

## Activity uploads (multipart/form-data)

The general proxy forwards binary uploads untouched — VirtualRow posts a FIT
file to `/proxy/api/v1/athlete/0/activities?name=…&external_id=…` with a
bearer token, and no upload-specific routing is needed:

- the body is read as `arrayBuffer()`, so the multipart boundary and the FIT
  bytes survive intact;
- `content-type` is forwarded, so the boundary parameter reaches upstream;
- `url.search` is appended to the target, so the query string is preserved.

Every deploy re-verifies this (see `deploy-intervals-proxy.yml`) by posting a
binary part with a deliberately invalid bearer token from each browser origin;
intervals.icu answers `401` before creating anything, so the probe is
non-mutating:

```bash
printf '\x0e\x10\x20\x00.FIT\x00\x00' > probe.fit
curl -i -X POST \
  -H 'Origin: http://localhost:5173' \
  -H 'Authorization: Bearer not-a-real-token' \
  -F 'file=@probe.fit;type=application/octet-stream' \
  'https://mt-intervals-proxy.intervals-login.workers.dev/proxy/api/v1/athlete/0/activities?name=probe&external_id=probe'
```

A `401` with `Via: 1.1 Caddy` is upstream answering; a `403` means the origin
is not allow-listed.

---

## Security

- **OAuth token endpoint (`/proxy/api/oauth/token`)**: open to any caller —
  `Access-Control-Allow-Origin: *` ensures every frontend can read both
  success and error bodies.  The `client_secret` is looked up from KV and
  never echoed back to callers.  Unknown `client_id` values receive a 401.
- **General proxy (`/proxy/*`)**: restricted to known browser origins and the
  desktop `X-MT-Client: desktop` header to prevent this worker from acting as
  a fully open relay.  This is **not** a hard security boundary — it is
  anti-abuse rate-limiting.  Allowed origins:

  | Origin | Used by |
  |--------|---------|
  | `https://maximumtrainer.github.io` | the deployed WASM app |
  | `http://localhost:8080`, `http://127.0.0.1:8080` | local WASM builds |
  | `http://localhost:5173`, `http://127.0.0.1:5173` | Vite dev servers (VirtualRow) |
  | `http://localhost:5500` | VS Code Live Server |

  Vite answers on both `localhost` and `127.0.0.1`, which are distinct origins,
  so both spellings of a dev port are listed.

  A rejected origin gets `403 Forbidden` **with** CORS headers, so the browser
  can read the status and report a configuration problem rather than a bare
  `TypeError: Failed to fetch` (#359).
- Only the headers the API actually needs (`Authorization`, `Content-Type`,
  `Accept`) are forwarded upstream.  `X-MT-Client` is consumed by the
  worker and not forwarded to intervals.icu.

---

## Cloudflare free tier limits

| Resource | Free limit |
|----------|-----------|
| Requests/day | 100,000 |
| CPU time/request | 10 ms |
| Workers | 100 |
| KV reads/day | 100,000 |
| KV writes/day | 1,000 |

A typical session (login + 3–5 API calls) uses well under the daily limit
for any realistic user volume.
