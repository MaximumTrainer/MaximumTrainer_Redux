# Strava integration — plan to make auto-upload work again

Status: **not implemented.** This PR only reworks the Preferences UI (the
"Auto Upload" page is now a Strava-only page). The actual OAuth + upload flow
is currently broken. This document records how to fix it.

## Why it's broken today

1. **The old flow depended on a maximumtrainer.com backend.** The authorize
   URL (`Environnement::getURLStravaAuthorize`, `environnement.cpp`) redirects
   to `…/strava_token_exchange`, where the maximumtrainer.com server performed
   the secret half of the OAuth handshake and returned the tokens to the
   embedded webview. That backend is defunct, so the handshake dies there.
2. **The redirect points at the dead backend, and the scope is stale.** The
   authorize URL still requests the deprecated `scope=write` and redirects to
   the maximumtrainer.com endpoint above.

> **Update (2026-06-08):** the legacy MaximumTrainer Strava app
> (**`client_id=7252`**) is still owned/accessible — the client secret can be
> shown/regenerated. So we **reuse 7252**; do **not** create a new app — under
> Strava's current API agreement a brand-new app also starts capped at **1**
> athlete (and 0 connected, no track record), so a new app is strictly worse.
> See "Athlete limit" below.

## Target: many users (this is a distributed app)

This app ships to many users, so the plan must be the **multi-user**
architecture: a secret-holding backend (Cloudflare Worker) + per-user OAuth.
The "personal / embed-the-secret" shortcut is **not** an option here — a
`client_secret` baked into a distributed binary can be extracted by anyone,
and a leaked Strava secret gets the app revoked for *all* users. It is kept
below only as a contrast.

## The hard constraint (confirmed against Strava docs)

Sources: <https://developers.strava.com/docs/authentication/> and
<https://developers.strava.com/docs/uploads/>.

Strava OAuth **always requires the `client_secret`** — it is a *required*
parameter of `POST /oauth/token` for **both** the `authorization_code`
exchange **and** `refresh_token` refresh. There is **no** PKCE
(`code_challenge`/`code_verifier`), **no** implicit flow, and **no** device
flow — only the standard confidential-client three-legged authorization-code
flow. So "just let the user log in without a secret" is impossible: login
yields a `code`, and turning that code into tokens needs the secret. The only
question is *where the secret lives*, and for a distributed app the answer is
"server-side, never in the binary."

## Athlete limit — the gating prerequisite (external, not code)

App 7252's settings show **"Number of athletes allowed to connect: 1"**. This
is Strava's cap for apps not approved under their current API agreement. The
"~751 currently connected" are *legacy* connections and do **not** lift the
cap — with the cap at 1, **new users cannot authorize at all** (the OAuth
screen rejects them). So nothing we build functions for multiple users until
this is raised.

This is **resolved with Strava, not in code**:

- Request an athlete-limit (and rate-limit) increase on **7252** — via the
  "request an increase" path in the app's API settings, or by emailing
  **developers@strava.com** referencing app 7252. Pitch: an existing
  indoor-training app that uploads users' own completed activities
  (`activity:write`); ask to restore/raise the athlete cap from 1.
- **Use 7252, not a new app** — a legacy app with 751 athletes has far better
  odds than a fresh registration (which starts capped at 1, 0 connected).
- Strava has become strict about these increases, so approval is **not
  guaranteed**. **Do this first** — the implementation below is not worth
  building until the cap is lifted.

> *Personal-only contrast (not our case):* if this were just the owner's own
> account, you could embed the secret at build time (no Worker) and skip the
> approval (owner = athlete #1). We are distributing, so this does not apply.

## What already exists (≈90% done)

- `StravaService` (`persistence/db/strava_service.{h,cpp}`) —
  `uploadActivity()`, `checkUploadStatus()`, `refreshToken()`,
  `deauthorize()` are all implemented.
- `DialogInfoWebView` — embedded `QWebEngineView` that watches for the OAuth
  redirect (already used for Strava + Intervals.icu).
- Encrypted token storage via `CredentialStore`
  (`strava_access_token` / `strava_refresh_token` on `Account`).
- The **Cloudflare Worker** proxy pattern used by Intervals.icu
  (`workers/intervals-cors-proxy/worker.js`) + a GitHub Pages OAuth callback
  page on `maximumtrainer.github.io`.

The Intervals.icu integration already solves the identical "hold a secret /
exchange a code" problem. Strava should reuse that pattern rather than invent
a new one.

## The fix (multi-user, reusing app 7252)

Only worth building **after** the athlete-limit increase above is granted.

1. **Deploy a `strava-token` Cloudflare Worker** (clone the Intervals one). It
   holds the `client_secret` as a Worker secret (never shipped to clients) and
   exposes one endpoint that forwards both the `authorization_code` exchange
   and `refresh_token` refresh to `https://www.strava.com/oauth/token`, with
   the secret injected server-side. Point a `URL_TOKEN_STRAVA` constant at it.
   The Worker is stateless — each user's app stores its own tokens.
2. **Repoint the in-app OAuth flow** (off the dead maximumtrainer.com endpoint):
   - authorize URL: keep `client_id=7252`, set `redirect_uri` to a detectable
     callback (the github.io callback page the Intervals flow already uses),
     and **`scope=activity:write`** (replace the deprecated `scope=write`).
     Set the app's *Authorization Callback Domain* in Strava settings to match.
   - `DialogInfoWebView` catches the redirect, extracts the `code`, POSTs it to
     the Worker (no secret in the app), and stores the returned
     access/refresh/expiry tokens per user.
3. **Wire auto-upload:** on workout finish, if a Strava token exists and an
   "auto-upload to Strava" toggle is on, refresh-if-expired (via the Worker),
   then call `StravaService::uploadActivity()` and poll status. Today only
   Intervals.icu auto-uploads (`MainWindow::checkToUploadFile`); add the Strava
   branch + a toggle on `Account` (`strava_auto_upload`).

## Upload flow (confirmed against docs — already implemented)

`StravaService::uploadActivity()` matches the docs exactly:
`POST https://www.strava.com/api/v3/uploads` (multipart) with `file` +
`data_type=fit`, optional `name`/`description`/`trainer=1`/`external_id`,
header `Authorization: Bearer <token>`; then poll
`GET /api/v3/uploads/:id` (~1 s interval) until `activity_id` is populated or
`error` is set. The upload code needs **no changes**.

## Scopes & gotchas

- **Scope:** `activity:write` is required to upload (the user grants it on the
  Strava consent screen; `write` is the deprecated old scope name).
- **Token lifetime:** access tokens expire every **6 hours** — always
  refresh-if-expired before an upload (`StravaService::refreshToken`, check
  `expires_at`).
- **Refresh-token rotation:** "Once a new refresh token has been returned, the
  older no longer works." Always **persist whatever `refresh_token` comes back**
  from a refresh (Strava usually returns the same one, but don't assume).
- **Rate limits are per-app, pooled across ALL users:** ~**200 req / 15 min,
  2,000 / day** overall (from 7252's settings). An upload is ~1 POST + a few
  status polls (~4 requests), so the default ceiling is only **~500 uploads/day
  across the whole userbase** — request a rate-limit increase together with the
  athlete-limit increase.

## Estimated work

External (do first, gates everything): get Strava to raise 7252's athlete +
rate limits.

In-app, once unblocked: updated authorize URL + `activity:write` scope,
`DialogInfoWebView` Strava callback handling, `URL_TOKEN_STRAVA`
exchange/refresh via the Worker, `strava_auto_upload` toggle + post-save
trigger. Out-of-app: scaffold + deploy the `strava-token` Worker with the
`client_secret`. The upload code (`StravaService`) needs no changes.
