# Strava Auto-Upload — implementation plan

Goal: after a workout finishes, upload the `.fit` to the rider's Strava
account — per user, with a one-time in-app "Connect with Strava" login and an
"auto-upload to Strava" toggle. The `client_secret` is stored in **GitHub**
(Actions secret) and injected at build time, mirroring the existing
Intervals.icu / TrainingPeaks secret pattern in this repo.

Status: **not implemented** — this document is the plan.

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

## 2. Where the `client_secret` lives — on GitHub

Strava **requires** `client_secret` for both the authorization-code exchange
and token refresh (no PKCE, no implicit/device flow — confirmed against
developers.strava.com/docs). So the secret must exist at runtime somewhere.

**Chosen approach — GitHub Actions secret + build-time injection:**

1. Add a repository **Actions secret**: `STRAVA_CLIENT_SECRET` (Settings →
   Secrets and variables → Actions).
2. Inject it into the build via `MaximumTrainer.pro` `DEFINES`, copying the
   existing block for `INTERVALS_OAUTH_CLIENT_SECRET` (`MaximumTrainer.pro`
   lines ~53–57). The CI workflows (`build-linux/mac/windows.yml`) already
   forward such secrets to the qmake step — add `STRAVA_CLIENT_SECRET` there.
3. The app reads it via a compiled-in constant (e.g. `STRAVA_CLIENT_SECRET`
   macro → `Environnement::CLIENT_SECRET_STRAVA`), defaulting to `""` for local
   dev builds.

**Trade-off (explicit):** this bakes the secret into the released binary, so a
determined user can extract it. This is the **same risk profile the repo
already accepts for Intervals.icu**. It does **not** expose any user's data —
OAuth still requires each user to authorize — the practical risk is only
rate-limit / app-identity abuse. Acceptable for this use case.

**Why not "secret on GitHub" without embedding?** GitHub can't host a live
secret-holding endpoint (Pages is static; Actions isn't a request-time API).
The only way to keep the secret out of the binary is a live backend — e.g. a
**Cloudflare Worker** (secret stored in Cloudflare via `wrangler secret`, the
Worker *code* in this GitHub repo, like `workers/intervals-cors-proxy/`). That
is the more-secure alternative if the embed risk ever becomes a concern; it is
**not** the plan here but is recorded as the fallback.

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

## 8. Order of work (prerequisites first)

1. ✅ **Upgrade app 7252** to 10 athletes (done 2026-06-08).
2. ⬜ Set the **Authorization Callback Domain** in Strava settings (Edit → must
   match the redirect host chosen in §3, e.g. `maximumtrainer.github.io`).
3. ⬜ Add **`STRAVA_CLIENT_SECRET`** to the repo's GitHub Actions secrets.
4. ⬜ Implement the in-app flow (sections 2–5). Can be coded now without the
   secret (the build-time constant defaults to `""`); the real secret is only
   needed in CI release builds and at the user's runtime.
5. ⬜ Verify end-to-end with a real ride; if >10 users are expected, file the
   Strava limit-increase request in parallel.
