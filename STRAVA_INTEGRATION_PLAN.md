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
2. **`client_id=7252` is the legacy MaximumTrainer Strava app** (hard-coded in
   `environnement.h`). If that API application is no longer owned/accessible,
   it's effectively dead and must be replaced with a freshly registered one.

## The hard constraint

Strava OAuth **always requires the `client_secret`** to exchange the login
`code` for tokens and to refresh them. Strava does **not** support PKCE /
public clients. A `client_secret` must never ship inside a distributed desktop
binary, so something server-side has to hold the secret and perform the
exchange.

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

## The fix

1. **Register a new Strava API application** (owned by us) at
   `https://www.strava.com/settings/api`. Note the `client_id` /
   `client_secret`, and set **Authorization Callback Domain** to
   `maximumtrainer.github.io` (where the OAuth callback page already lives).
2. **Deploy a small `strava-token` Cloudflare Worker** (clone the Intervals
   one). It holds the `client_secret` as a Worker secret (never shipped to
   clients) and exposes one endpoint that forwards both the
   `authorization_code` exchange and `refresh_token` requests to
   `https://www.strava.com/oauth/token` with the secret injected server-side.
   Add a `URL_TOKEN_STRAVA` constant pointing at it.
3. **Repoint the in-app OAuth flow:**
   - authorize URL uses *our* new `client_id`, `redirect_uri` = the
     github.io callback page, and **`scope=activity:write`**
     (the current URL uses the deprecated `scope=write` — must be updated).
   - `DialogInfoWebView` catches the redirect, extracts the `code`, POSTs it
     to the Worker (no secret in the app), and stores the returned
     access/refresh/expiry tokens.
4. **Wire auto-upload:** on workout finish, if a Strava token exists and an
   "auto-upload to Strava" toggle is on, refresh-if-expired via the Worker,
   then call `StravaService::uploadActivity()` and poll status. Today only
   Intervals.icu auto-uploads (`MainWindow::checkToUploadFile`); add the
   Strava branch + a toggle on `Account` (`strava_auto_upload`).

## Scopes & gotchas

- **Scope:** `activity:write` is required to upload. The user grants it on the
  Strava consent screen. (`write` is the deprecated old scope name.)
- **Rate limits / distribution:** a personal Strava app works immediately for
  the owner's own account. Distributing to many users hits per-app rate limits
  (100 req / 15 min, 1000 / day) and may require Strava app review for higher
  limits — fine for personal/small use, flag if going wide.
- **Token lifetime:** Strava access tokens expire every 6 hours; always
  refresh-if-expired before an upload (`StravaService::refreshToken`).

## Estimated work

In-app (one branch): new authorize URL + scope, `DialogInfoWebView` Strava
callback handling, `URL_TOKEN_STRAVA` exchange/refresh, `strava_auto_upload`
toggle + post-save trigger. Out-of-app: register the Strava app, scaffold +
deploy the Worker with the `client_secret`. The upload code itself needs no
changes.
