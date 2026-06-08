# Maximum Trainer — Strava OAuth token-exchange Worker

A tiny Cloudflare Worker that holds the Strava **client_secret** server-side so
it never ships inside the distributed app binary.

## Why this exists

Strava requires `client_secret` for **both** the authorization-code exchange
**and** the token refresh, and offers **no PKCE / public-client flow** (see
`STRAVA_INTEGRATION_PLAN.md`). The only way to keep the secret out of the
shipped binary is a secret-holding endpoint. The app sends the authorization
`code` (or a `refresh_token`); this worker injects `client_id` + `client_secret`
and forwards to `https://www.strava.com/oauth/token`.

This mirrors `workers/intervals-cors-proxy`, with one difference: the Intervals
worker is a transparent CORS proxy, while this one **injects the credentials**
(the secret is never sent by the app).

## Endpoint

```
POST /strava/oauth/token        (application/x-www-form-urlencoded)
  authorization_code:  grant_type=authorization_code, code, redirect_uri
  refresh_token:       grant_type=refresh_token, refresh_token
```

The worker adds `client_id` + `client_secret` and returns Strava's token JSON
(`access_token`, `refresh_token`, `expires_at`, …).

Access is limited to the allow-listed browser origins (the github.io WASM app /
localhost) and native desktop clients that send `X-MT-Client: desktop`, so it
can't be used as a public relay for the secret.

## Deploy

```bash
cd workers/strava-token-proxy
npx wrangler login          # once
npx wrangler deploy
npx wrangler secret put STRAVA_CLIENT_SECRET   # paste the (rotated) Strava secret
```

`STRAVA_CLIENT_ID` is a non-secret var in `wrangler.toml` (7252) and can stay
there. **Never** put `STRAVA_CLIENT_SECRET` in `wrangler.toml` or commit it.

## After deploying

`wrangler deploy` prints the worker URL, e.g.
`https://mt-strava-token.<your-subdomain>.workers.dev`. The token endpoint is
that URL + `/strava/oauth/token`. Put it into `URL_TOKEN_STRAVA` in
`src/persistence/db/environnement.h` (or wire it via a build var, the same way
`INTERVALS_PROXY_URL` is handled for the WASM build).
