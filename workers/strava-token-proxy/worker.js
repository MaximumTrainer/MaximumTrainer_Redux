/**
 * Maximum Trainer — Strava OAuth token-exchange Worker
 *
 * Holds the Strava client_id + client_secret server-side so the secret never
 * ships inside the distributed app binary. The app POSTs the authorization
 * `code` (or a `refresh_token`); this worker injects the credentials and
 * forwards to https://www.strava.com/oauth/token, returning the token JSON.
 *
 * Why a worker (vs. embedding the secret): Strava requires client_secret for
 * BOTH the authorization_code exchange and the refresh, and offers no PKCE /
 * public-client flow. A secret-holding endpoint is the only way to keep the
 * secret out of the binary. This mirrors workers/intervals-cors-proxy.
 *
 * Routing:
 *   POST /strava/oauth/token   (application/x-www-form-urlencoded body)
 *     authorization_code:  grant_type=authorization_code, code, redirect_uri
 *     refresh_token:       grant_type=refresh_token, refresh_token
 *
 * Config (set via wrangler — never commit the secret):
 *   STRAVA_CLIENT_ID      plain var in wrangler.toml (7252; public, not secret)
 *   STRAVA_CLIENT_SECRET  `npx wrangler secret put STRAVA_CLIENT_SECRET`
 *
 * Deployment:
 *   cd workers/strava-token-proxy
 *   npx wrangler deploy
 *   npx wrangler secret put STRAVA_CLIENT_SECRET
 * Then put the resulting https://<name>.<subdomain>.workers.dev URL into
 * URL_TOKEN_STRAVA in src/persistence/db/environnement.h.
 */

const STRAVA_TOKEN_URL = 'https://www.strava.com/oauth/token';
const TOKEN_PATH = '/strava/oauth/token';

// Browser origins (WASM build / local dev) allowed to read responses via CORS.
const ALLOWED_ORIGINS = [
  'https://maximumtrainer.github.io',
  'http://localhost:8080',
  'http://localhost:5173',
  'http://localhost:5500',
  'http://127.0.0.1:8080',
];

// Native desktop clients are not browsers and send no Origin; they identify
// with X-MT-Client instead. Not a security boundary (any client can set it) —
// it just keeps the worker from acting as a fully open relay. Must match the
// C++ constants INTERVALS_PROXY_CLIENT_HEADER / *_DESKTOP_CLIENT_VALUE.
const CLIENT_HEADER = 'x-mt-client';
const ALLOWED_CLIENTS = ['desktop'];

function corsHeaders(origin) {
  return {
    'Access-Control-Allow-Origin': origin,
    'Access-Control-Allow-Methods': 'POST, OPTIONS',
    'Access-Control-Allow-Headers': 'content-type, x-mt-client',
    'Access-Control-Max-Age': '86400',
    'Vary': 'Origin',
  };
}

function jsonError(code, status, headers) {
  return new Response(JSON.stringify({ error: code }), {
    status,
    headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store', ...headers },
  });
}

export default {
  async fetch(request, env) {
    const origin = request.headers.get('Origin') || '';
    const client = (request.headers.get(CLIENT_HEADER) || '').toLowerCase();
    const isBrowser = origin !== '' && ALLOWED_ORIGINS.includes(origin);
    const isDesktop = origin === '' && ALLOWED_CLIENTS.includes(client);

    // Reject anything that is neither a known browser origin nor a known
    // desktop client, so this never becomes a public open relay for the secret.
    if (!isBrowser && !isDesktop) {
      return new Response('Forbidden', { status: 403 });
    }
    const cors = isBrowser ? corsHeaders(origin) : {};

    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: corsHeaders(origin) });
    }

    const url = new URL(request.url);
    if (request.method !== 'POST' || url.pathname !== TOKEN_PATH) {
      return new Response('Not Found', { status: 404, headers: cors });
    }

    if (!env.STRAVA_CLIENT_ID || !env.STRAVA_CLIENT_SECRET) {
      return jsonError('worker_misconfigured', 500, cors);
    }

    // Read the app's params and forward ONLY the expected fields, with the
    // credentials injected here. Never echo the client_secret back.
    const inForm = new URLSearchParams(await request.text());
    const grantType = inForm.get('grant_type');

    const body = new URLSearchParams();
    body.set('client_id', env.STRAVA_CLIENT_ID);
    body.set('client_secret', env.STRAVA_CLIENT_SECRET);
    body.set('grant_type', grantType || '');

    if (grantType === 'authorization_code') {
      body.set('code', inForm.get('code') || '');
      const redirectUri = inForm.get('redirect_uri');
      if (redirectUri) body.set('redirect_uri', redirectUri);
    } else if (grantType === 'refresh_token') {
      body.set('refresh_token', inForm.get('refresh_token') || '');
    } else {
      return jsonError('unsupported_grant_type', 400, cors);
    }

    const upstream = await fetch(STRAVA_TOKEN_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        'Accept': 'application/json',
      },
      body: body.toString(),
    });

    const headers = new Headers(cors);
    headers.set('Content-Type', 'application/json');
    headers.set('Cache-Control', 'no-store');
    return new Response(upstream.body, {
      status: upstream.status,
      statusText: upstream.statusText,
      headers,
    });
  },
};
