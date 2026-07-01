/**
 * Maximum Trainer — Intervals.icu CORS Proxy Worker
 *
 * Forwards requests from the WASM app (https://maximumtrainer.github.io)
 * to https://intervals.icu, injecting CORS headers so that the browser's
 * same-origin policy is satisfied.
 *
 * intervals.icu does not natively emit CORS headers (confirmed: OPTIONS to
 * /oauth/token returns HTTP 405 with no Access-Control-Allow-Origin).  All
 * Qt WASM network requests go through Emscripten XMLHttpRequest, which is
 * subject to browser CORS enforcement, so this proxy is the only path that
 * makes the GitHub Pages WASM app functional.
 *
 * Routing:
 *   /proxy/<path>?<query>  →  https://intervals.icu/<path>?<query>
 *
 * OAuth token exchange (POST /proxy/api/oauth/token):
 *   Accepts a JSON body with { code, redirect_uri } from any frontend
 *   (WASM, VirtualRow, maximum_trainer, etc.).  The worker constructs the
 *   outbound application/x-www-form-urlencoded payload, appending
 *   grant_type=authorization_code, client_id, and client_secret from env
 *   so that neither the WASM binary nor the desktop app carry the secret.
 *
 *   Also accepts a JSON body with { refresh_token } for the refresh flow.
 *
 *   For backwards-compatibility, application/x-www-form-urlencoded input
 *   is also accepted; the worker extracts code/redirect_uri/refresh_token
 *   and rebuilds the full payload from env credentials.
 *
 * Security:
 *   - CORS is unrestricted (Access-Control-Allow-Origin: *) so every
 *     frontend (WASM, VirtualRow, etc.) can read both success and error
 *     responses.  The client_id and client_secret are never echoed back.
 *   - The general proxy (/proxy/* except the token endpoint) is restricted
 *     to known browser origins and the desktop X-MT-Client header to
 *     prevent this worker from acting as a fully open relay.
 *   - Only the headers the API actually needs are forwarded upstream.
 *
 * Deployment:
 *   cd workers/intervals-cors-proxy
 *   npx wrangler deploy
 *   npx wrangler secret put INTERVALS_CLIENT_SECRET
 *
 * After deployment the worker URL (https://<name>.<subdomain>.workers.dev)
 * must be set as the INTERVALS_PROXY_URL repository variable in GitHub so
 * that pages.yml injects it into docs/app/index.html.
 */

// Browser origins (real web origins) that are allowed to use the general
// proxy.  The OAuth token endpoint is open to any origin (uses * CORS).
const ALLOWED_ORIGINS = [
  'https://maximumtrainer.github.io',
  'http://localhost:8080',
  'http://localhost:5173',
  'http://localhost:5500',
  'http://127.0.0.1:8080',
];

// Native desktop clients (Qt Widgets build) are not browsers and are not
// subject to CORS.  They identify themselves by sending an X-MT-Client
// header instead of an Origin.  Must match the C++ constants
// INTERVALS_PROXY_CLIENT_HEADER / INTERVALS_PROXY_DESKTOP_CLIENT_VALUE in
// src/persistence/db/environnement.h.
const CLIENT_HEADER = 'x-mt-client';
const ALLOWED_CLIENTS = ['desktop'];

const ICU_BASE = 'https://intervals.icu';
const ICU_TOKEN_PATH = '/api/oauth/token';
const ICU_TOKEN_URL  = ICU_BASE + ICU_TOKEN_PATH;

// Headers forwarded to intervals.icu — only those the API actually needs.
const FORWARD_HEADERS = [
  'authorization',
  'content-type',
  'accept',
  'accept-encoding',
  'accept-language',
];

// Standard CORS headers returned on every response so that any frontend
// (WASM, VirtualRow, maximum_trainer, …) can read both success and error
// bodies without being blocked by the browser's same-origin policy.
const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, PATCH, OPTIONS',
  'Access-Control-Allow-Headers': 'authorization, content-type, accept, x-mt-client',
  'Access-Control-Max-Age': '86400',
};

function jsonError(code, status) {
  return new Response(JSON.stringify({ error: code }), {
    status,
    headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store', ...CORS_HEADERS },
  });
}

function forbidden() {
  return new Response('Forbidden', { status: 403 });
}

export default {
  async fetch(request, env, _ctx) {
    // OPTIONS preflight — always allowed with wildcard CORS so any frontend
    // (WASM, VirtualRow, maximum_trainer) can reach this worker.
    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: CORS_HEADERS });
    }

    const url = new URL(request.url);

    // ── OAuth token exchange ───────────────────────────────────────────────
    // Handled before the origin/client allow-list check so that any frontend
    // can exchange an authorization code or refresh a token.  The client_id
    // and client_secret are injected here server-side and never sent to or
    // echoed back to callers.
    if (request.method === 'POST' && url.pathname === '/proxy' + ICU_TOKEN_PATH) {
      try {
        if (!env.INTERVALS_CLIENT_ID || !env.INTERVALS_CLIENT_SECRET) {
          return jsonError('worker_misconfigured', 500);
        }

        // Accept both JSON and application/x-www-form-urlencoded input.
        const contentType = (request.headers.get('content-type') || '').toLowerCase();
        let code, redirectUri, refreshToken, grantType;

        if (contentType.includes('application/json')) {
          const json = await request.json();
          code         = json.code;
          redirectUri  = json.redirect_uri;
          refreshToken = json.refresh_token;
          grantType    = json.grant_type;
        } else {
          const form = new URLSearchParams(await request.text());
          code         = form.get('code');
          redirectUri  = form.get('redirect_uri');
          refreshToken = form.get('refresh_token');
          grantType    = form.get('grant_type');
        }

        // Infer grant type from the fields present when the caller omits it.
        if (!grantType) {
          grantType = refreshToken ? 'refresh_token' : 'authorization_code';
        }

        const params = new URLSearchParams();
        params.set('grant_type',    grantType);
        params.set('client_id',     env.INTERVALS_CLIENT_ID);
        params.set('client_secret', env.INTERVALS_CLIENT_SECRET);

        if (grantType === 'authorization_code') {
          if (code)        params.set('code',         code);
          if (redirectUri) params.set('redirect_uri', redirectUri);
        } else if (grantType === 'refresh_token') {
          if (refreshToken) params.set('refresh_token', refreshToken);
        } else {
          return jsonError('unsupported_grant_type', 400);
        }

        const upstream = await fetch(ICU_TOKEN_URL, {
          method:  'POST',
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'Accept':       'application/json',
          },
          body: params.toString(),
        });

        const headers = new Headers(CORS_HEADERS);
        headers.set('Content-Type',  'application/json');
        headers.set('Cache-Control', 'no-store');
        return new Response(upstream.body, {
          status:     upstream.status,
          statusText: upstream.statusText,
          headers,
        });
      } catch (err) {
        console.error('intervals token exchange error:', err);
        return jsonError('internal_error', 500);
      }
    }

    // ── General proxy ──────────────────────────────────────────────────────
    // Restricted to known browser origins and desktop clients to prevent
    // this worker from acting as a fully open relay.
    const origin = request.headers.get('Origin') || '';
    const client = (request.headers.get(CLIENT_HEADER) || '').toLowerCase();
    const isBrowser = origin !== '' && ALLOWED_ORIGINS.includes(origin);
    const isDesktop = origin === '' && ALLOWED_CLIENTS.includes(client);

    if (!isBrowser && !isDesktop) {
      return forbidden();
    }

    if (!url.pathname.startsWith('/proxy/')) {
      return new Response('Not Found', {
        status: 404,
        headers: { ...CORS_HEADERS, 'Cache-Control': 'no-store' },
      });
    }

    // Strip /proxy prefix, keep the rest of the path and query string.
    const targetPath = url.pathname.slice('/proxy'.length);
    const targetUrl  = ICU_BASE + targetPath + url.search;

    // Build a clean forwarded request with only the necessary headers.
    // The X-MT-Client marker is NOT forwarded upstream — it is purely a
    // worker-side allow-list signal.
    const forwardHeaders = new Headers();
    for (const name of FORWARD_HEADERS) {
      const value = request.headers.get(name);
      if (value) forwardHeaders.set(name, value);
    }

    const isBodyMethod = !['GET', 'HEAD'].includes(request.method);
    const body = isBodyMethod ? await request.arrayBuffer() : undefined;

    const proxyReq = new Request(targetUrl, {
      method:   request.method,
      headers:  forwardHeaders,
      body,
      redirect: 'follow',
    });

    try {
      const response = await fetch(proxyReq);

      const responseHeaders = new Headers(response.headers);
      for (const [k, v] of Object.entries(CORS_HEADERS)) {
        responseHeaders.set(k, v);
      }
      // Prevent Cloudflare or the browser from caching authenticated responses.
      responseHeaders.set('Cache-Control', 'no-store');

      return new Response(response.body, {
        status:     response.status,
        statusText: response.statusText,
        headers:    responseHeaders,
      });
    } catch (err) {
      console.error('intervals proxy upstream error:', err);
      return jsonError('upstream_error', 502);
    }
  },
};
