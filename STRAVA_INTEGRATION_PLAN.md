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
> shown/regenerated. So we **reuse 7252**; no new app registration is needed.
> See "Athlete limit" below for the one real catch.

## The hard constraint

Strava OAuth **always requires the `client_secret`** to exchange the login
`code` for tokens and to refresh them. Strava does **not** support PKCE /
public clients. Where the secret lives depends on whether this is a
personal-only build or a distributed one (see "Two paths" below).

## Athlete limit (the real catch)

App 7252's settings show **"Number of athletes allowed to connect: 1"**. This
is Strava's cap for unapproved apps under their newer API agreement. The
"~751 currently connected" are *legacy* connections and do **not** lift the
cap. Consequence:

- **Personal use (the owner uploading their own rides):** works now — the
  owner is athlete #1, no approval needed.
- **Distributing to other users:** blocked until Strava raises the athlete
  limit via their (slow, strict) app-approval process.

## Two paths

| | Personal / desktop-only | Distribute to many users |
| --- | --- | --- |
| New app registration | not needed (reuse 7252) | not needed (reuse 7252) |
| `client_secret` location | **embed at build time** via the `.pro` env-var pattern already used for Intervals/TP (add `STRAVA_CLIENT_SECRET`) | **Cloudflare Worker** holds it (out of the binary; also gives CORS for WASM) |
| Cloudflare Worker | not needed | needed |
| Strava athlete cap | fine (owner is #1) | **must be raised by Strava** |

The personal path is much lighter: no Worker, no new app — just embed the
secret, fix scope/redirect, and wire the upload trigger.

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

Common to both paths (reusing app **7252**):

1. **Set the secret holder.**
   - *Personal:* add `STRAVA_CLIENT_SECRET` to the build via the `.pro`
     env-var pattern already used for `INTERVALS_OAUTH_CLIENT_SECRET` /
     `TP_CLIENT_SECRET`, and exchange the code directly from the app against
     `https://www.strava.com/oauth/token`.
   - *Distribute:* deploy a small `strava-token` Cloudflare Worker (clone the
     Intervals one) that holds the secret as a Worker secret and forwards both
     `authorization_code` exchange and `refresh_token` to Strava with the
     secret injected server-side. Point a `URL_TOKEN_STRAVA` constant at it.
2. **Repoint the in-app OAuth flow** (off the dead maximumtrainer.com endpoint):
   - authorize URL: keep `client_id=7252`, set `redirect_uri` to a detectable
     callback (e.g. the github.io callback page, or a localhost/`/exchange-token`
     path the embedded webview can catch), and **`scope=activity:write`**
     (replace the deprecated `scope=write`). Set the app's
     *Authorization Callback Domain* in Strava settings to match.
   - `DialogInfoWebView` catches the redirect, extracts the `code`, exchanges
     it (directly with the embedded secret, or via the Worker), and stores the
     returned access/refresh/expiry tokens.
3. **Wire auto-upload:** on workout finish, if a Strava token exists and an
   "auto-upload to Strava" toggle is on, refresh-if-expired, then call
   `StravaService::uploadActivity()` and poll status. Today only Intervals.icu
   auto-uploads (`MainWindow::checkToUploadFile`); add the Strava branch + a
   toggle on `Account` (`strava_auto_upload`).

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

Personal path (likely the target): in-app only — updated authorize URL +
`activity:write` scope, `DialogInfoWebView` Strava callback handling, direct
token exchange/refresh with a build-time `STRAVA_CLIENT_SECRET`,
`strava_auto_upload` toggle + post-save trigger. Out-of-app: just set the
`client_secret` and the Authorization Callback Domain in app 7252's settings.
The upload code (`StravaService`) needs no changes.

Distribute path adds: the `strava-token` Cloudflare Worker, and getting
Strava to raise the per-app athlete limit above 1.
