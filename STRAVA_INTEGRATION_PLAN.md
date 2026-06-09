# Strava Auto-Upload — implementation

Goal: after a workout finishes, upload the `.fit` to the rider's Strava
account — per user, with a one-time in-app "Connect with Strava" login and an
"auto-upload to Strava" toggle.

**Architecture: Plan B (production).** The `client_secret` lives **only in a
Cloudflare Worker** (`workers/strava-token-proxy/`), never in the app binary.
The app sends just the authorization `code` (or a `refresh_token`); the worker
injects the credentials and calls Strava's `/oauth/token`. This is the secure
path for a distributed app — Strava has no PKCE, so a secret-holder is required,
and a worker keeps it out of every shipped copy.

Status: **app side implemented** (branch `strava-oauth-auto-upload`). Remaining
work is operational: deploy the worker, set its secret, set the Strava callback
domain, then test. See "Deploy & test" below.

---

## 1. App registration (reuse 7252, self-serve upgrade)

- Reuse Strava app **Client ID 7252** (owner blais.maxime@gmail.com,
  Category "Indoor", 751 athletes connected). Do **not** create a new app — a
  fresh app starts capped at 1 athlete with no history; 7252 is the better base.
- **Athlete cap:** ✅ **upgraded (2026-06-08)** — now **10 athletes**, read
  200 req/15 min · 2,000/day, overall **400 req/15 min · 4,000/day**. (751
  legacy athletes remain "currently connected"; the 10 is the going-forward
  cap — verify with a real new authorization during testing.)
- **For more than 10 users:** the self-serve tier stops at 10. Going higher
  needs a manual increase request to Strava (developers@strava.com), which is
  reviewed and not guaranteed. Plan to launch at ≤10, request more if needed.
- **Scope:** the current token on the API page is `read`-only; uploads need
  `activity:write`, granted per user via the OAuth consent screen.

## 2. Where the `client_secret` lives — the Cloudflare Worker

Strava **requires** `client_secret` for both the authorization-code exchange
and token refresh (no PKCE, no implicit/device flow — confirmed against
developers.strava.com/docs). So a secret-holder must exist at runtime. For a
distributed app the secret must **not** ship in the binary, so it lives in a
small Cloudflare Worker:

- **`workers/strava-token-proxy/`** holds `STRAVA_CLIENT_SECRET` (set via
  `wrangler secret put`) and `STRAVA_CLIENT_ID` (`7252`, a plain var). It
  exposes `POST /strava/oauth/token`, injects the credentials, and forwards to
  `https://www.strava.com/oauth/token`.
- The app calls it at `URL_TOKEN_STRAVA`
  (`https://mt-strava-token.intervals-login.workers.dev/strava/oauth/token`,
  matching the existing Intervals worker subdomain) and sends only the
  `code` / `refresh_token`. The secret is never in the app, the build, or
  GitHub. **No `STRAVA_CLIENT_SECRET` GitHub Actions secret is needed** (that
  was the Plan A / embed approach we did not take).
- Access is allow-listed to the github.io WASM origin and native desktop
  clients (`X-MT-Client: desktop`), so it can't be used as an open relay.

## 3. Per-user OAuth flow

1. **"Connect with Strava"** button (Strava Preferences page) opens the
   authorize URL:
   `https://www.strava.com/oauth/authorize?client_id=7252&response_type=code&redirect_uri=<callback>&scope=activity:write&approval_prompt=auto`
   - Set the app's **Authorization Callback Domain** in Strava settings to the
     callback host (reuse the github.io OAuth callback page the Intervals flow
     already uses, with a Strava marker, or a dedicated detectable path).
   - Use `scope=activity:write` (replaces the dead `scope=write`).
2. Embedded `DialogInfoWebView` catches the redirect and extracts `code`.
3. **Exchange** (client-side, with the embedded secret):
   `POST https://www.strava.com/oauth/token` with `client_id`,
   `client_secret`, `code`, `grant_type=authorization_code`. Parse with
   `Util::parseJsonStravaObject` and store `access_token` / `refresh_token` /
   `expires_at` via `CredentialStore` (`Account::strava_*`).
4. **Refresh** when expired: same endpoint, `grant_type=refresh_token` +
   secret. `StravaService::refreshToken(clientId, clientSecret, refreshToken)`
   already exists; persist the (possibly rotated) refresh token afterward.

## 4. Upload (already implemented — no changes)

`StravaService::uploadActivity()` matches the docs:
`POST https://www.strava.com/api/v3/uploads` (multipart) with `file` +
`data_type=fit`, optional `name` / `description` / `trainer=1` /
`external_id`; `Authorization: Bearer <token>`. Then poll
`GET /api/v3/uploads/:id` (~1 s) until `activity_id` is set or `error` returns.

## 5. Auto-upload wiring

- Add `Account::strava_auto_upload` (bool) + load/save (QSettings), and an
  **"Auto-upload completed activities to Strava"** checkbox on the Strava
  Preferences page (`dialogmainwindowconfig`).
- In the post-save path (`MainWindow::checkToUploadFile`, where Intervals.icu
  auto-upload already lives): if `strava_auto_upload` and a Strava token
  exists → refresh-if-expired → `uploadActivity()` → poll status → toast the
  result. Mirror the existing Intervals branch.

## 6. Gotchas

- **Access token = 6 h lifetime** → always refresh-if-expired before upload
  (check `expires_at`).
- **Refresh-token rotation:** "once a new refresh token is returned, the old
  one stops working" — persist whatever comes back from each refresh.
- **Rate limits are pooled across all users:** post-upgrade ~4,000 req/day; an
  upload is ~1 POST + a few status polls (~4 req) → ~1,000 uploads/day total,
  ample for ≤10 users.
- **One-time re-auth at `activity:write`:** existing read-only tokens can't be
  upgraded in place; the user must authorize once with the write scope.

## 7. Files to touch

- `MaximumTrainer.pro` + `.github/workflows/build-*.yml` — inject
  `STRAVA_CLIENT_SECRET`.
- `src/persistence/db/environnement.{h,cpp}` — new authorize URL (scope
  `activity:write`, new `redirect_uri`), `client_secret` constant, token
  endpoint.
- `src/persistence/db/strava_service.{h,cpp}` or `extrequest.{h,cpp}` — add the
  `authorization_code` exchange (refresh already exists).
- `src/ui/dialoginfowebview.{h,cpp}` — Strava OAuth callback handling
  (re-add a Strava branch; some Strava handling was stripped in #244).
- `src/model/account.{h,cpp}` — `strava_auto_upload` field + persistence.
- `src/ui/dialogmainwindowconfig.{ui,cpp}` — auto-upload checkbox on the
  Strava page.
- `src/ui/mainwindow.cpp` — auto-upload trigger + finished-slot.

## 8. Deploy & test

App side is implemented (branch `strava-oauth-auto-upload`). Remaining steps:

1. ✅ **Upgrade app 7252** to 10 athletes (done 2026-06-08).
2. ✅ **Authorization Callback Domain** set to `maximumtrainer.github.io`.
3. ✅ App implementation (worker + OAuth exchange/refresh + auto-upload toggle
   + trigger).
4. ⬜ **Rotate the Strava client secret** (it was shown on screen once) — Strava
   API page → *Generate New Client Secret*.
5. ⬜ **Deploy the worker** and set its secret (needs Cloudflare access to the
   same account as the Intervals worker, subdomain `intervals-login`):
   ```bash
   cd workers/strava-token-proxy
   npx wrangler deploy
   npx wrangler secret put STRAVA_CLIENT_SECRET   # paste the rotated secret
   ```
   It must land at `https://mt-strava-token.intervals-login.workers.dev`
   (the name in `wrangler.toml`); that matches `URL_TOKEN_STRAVA` in the app.
   **No GitHub secret / build-time secret is needed** — the app never sees it.
6. ⬜ **Test locally:** build the app normally (no secret env), open
   Preferences → Strava → **Connect with Strava** → log in → grant
   `activity:write`. Confirm it links. Then tick **Auto-upload completed
   activities to Strava**, ride a (demo) workout, and confirm it uploads +
   appears on Strava.
7. ⬜ When you hit the 10-athlete cap, file the Strava limit-increase request
   for app 7252 (developers@strava.com).
