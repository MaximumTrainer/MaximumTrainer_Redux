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

### 4 — Deploy the worker

Either via GitHub Actions (automatic after setting the secrets — `pages.yml`
will deploy on every push to master) **or** manually:

```bash
cd workers/intervals-cors-proxy
npx wrangler deploy --account-id <YOUR_ACCOUNT_ID>
```

### 4b — Set the OAuth client_secret (REQUIRED for login)

intervals.icu **requires the `client_secret`** on `/api/oauth/token` requests
(HTTP 422 without it).  Store it as a Worker secret so the worker can inject
it server-side — neither the WASM nor the desktop binary then needs to carry
it:

```bash
cd workers/intervals-cors-proxy
npx wrangler secret put INTERVALS_CLIENT_SECRET
# paste the client_secret for intervals.icu OAuth client 259
```

The `deploy-intervals-proxy.yml` workflow does this automatically from the
`INTERVALS_OAUTH_CLIENT_SECRET` repository secret (and verifies the deployed
URL actually proxies), so a `workflow_dispatch` run of that workflow is the
easiest way to deploy correctly.

> ⚠️ Verify what is actually deployed (JSON body, no auth required):
> ```bash
> curl -s -X POST https://<worker-url>/proxy/api/oauth/token \
>   -H 'Content-Type: application/json' \
>   -d '{"code":"x","redirect_uri":"http://localhost:1/"}'
> ```
> should return an intervals.icu JSON error (e.g. *invalid code*), **not** an
> empty 404.  Legacy form-encoded input is also accepted for backwards
> compatibility.

### 5 — Note the worker URL

After deployment, Wrangler prints something like:

```
Published mt-intervals-proxy (0.12 sec)
  https://mt-intervals-proxy.<your-subdomain>.workers.dev
```

Copy that URL.

### 6 — Set `INTERVALS_PROXY_URL` in GitHub

Repo → Settings → Secrets and variables → Actions → Variables tab:

| Name | Value |
|------|-------|
| `INTERVALS_PROXY_URL` | `https://mt-intervals-proxy.<your-subdomain>.workers.dev` |

### 7 — Trigger a pages.yml re-deploy

Push any change to `docs/` or `master`, or manually trigger the
"Deploy GitHub Pages" workflow.  The workflow injects `INTERVALS_PROXY_URL`
into `docs/app/index.html` before deploying, enabling the WASM app to
proxy all `intervals.icu` requests through the Worker.

## OAuth token exchange

The dedicated token endpoint (`POST /proxy/api/oauth/token`) is open to any
caller — no `Origin` or `X-MT-Client` header required.  The worker constructs
the outbound `application/x-www-form-urlencoded` payload from env variables so
callers only need to supply `code` and `redirect_uri`:

**JSON input (preferred):**
```json
{ "code": "<authorization_code>", "redirect_uri": "<redirect_uri>" }
```

**Form-encoded input (legacy / desktop client):**
```
grant_type=authorization_code&code=<code>&redirect_uri=<redirect_uri>
```

**Refresh token (JSON or form-encoded):**
```json
{ "grant_type": "refresh_token", "refresh_token": "<token>" }
```

The worker appends `grant_type=authorization_code`, `client_id`, and
`client_secret` from `env` before forwarding to
`https://intervals.icu/api/oauth/token`.

---

## Request routing

```
Any frontend (WASM, VirtualRow, maximum_trainer, …)
  → https://mt-intervals-proxy.<subdomain>.workers.dev/proxy/api/oauth/token
  → Cloudflare Worker (builds form body, injects credentials, adds CORS *)
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

## Security

- **OAuth token endpoint (`/proxy/api/oauth/token`)**: open to any caller —
  `Access-Control-Allow-Origin: *` ensures every frontend can read both
  success and error bodies.  The `client_id` and `client_secret` are never
  echoed back to callers.
- **General proxy (`/proxy/*`)**: restricted to known browser origins
  (`https://maximumtrainer.github.io` and localhost) and the desktop
  `X-MT-Client: desktop` header to prevent this worker from acting as a
  fully open relay.  This is **not** a hard security boundary — it is
  anti-abuse rate-limiting.
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

A typical session (login + 3–5 API calls) uses well under the daily limit
for any realistic user volume.
