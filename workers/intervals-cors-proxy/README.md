# Intervals.icu CORS Proxy — Cloudflare Worker

Forwards requests from the Maximum Trainer WASM app
(`https://maximumtrainer.github.io`) to `https://intervals.icu` with CORS
headers, because intervals.icu does not natively support cross-origin browser
requests (confirmed: `OPTIONS /oauth/token` → HTTP 405, no CORS headers).

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

---

## Request routing

```
WASM app (browser)
  → https://mt-intervals-proxy.<subdomain>.workers.dev/proxy/oauth/token
  → Cloudflare Worker (adds CORS headers)
  → https://intervals.icu/oauth/token
```

The `index.html` fetch/XHR interceptor rewrites every request whose URL
starts with `https://intervals.icu` to the proxy before any network call
is made.

---

## Security

- Only `https://maximumtrainer.github.io` (and localhost for development)
  can read proxied responses via the browser's CORS policy.
- The native desktop client identifies itself with a sentinel
  `Origin: https://maximumtrainer-desktop.invalid` header (see
  `INTERVALS_PROXY_DESKTOP_ORIGIN` in `src/persistence/db/environnement.h`).
  This is **not** a security boundary — anyone can forge an `Origin` header
  from a non-browser HTTP client — it just keeps the worker's allow-list
  honest about which clients are expected.
- Requests from other origins receive `HTTP 403`.
- Only the headers the API actually needs (`Authorization`, `Content-Type`,
  `Accept`) are forwarded upstream.

---

## Cloudflare free tier limits

| Resource | Free limit |
|----------|-----------|
| Requests/day | 100,000 |
| CPU time/request | 10 ms |
| Workers | 100 |

A typical session (login + 3–5 API calls) uses well under the daily limit
for any realistic user volume.
