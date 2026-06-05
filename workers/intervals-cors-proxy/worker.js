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
 * Security:
 *   - Only the allowed origin list can read responses via the browser.
 *   - Requests from other origins (including absent-Origin server calls)
 *     receive a 403 to prevent this from becoming a public open proxy.
 *   - Only the headers needed for OAuth and API access are forwarded.
 *
 * Deployment:
 *   cd workers/intervals-cors-proxy
 *   npx wrangler deploy
 *
 * After deployment the worker URL (https://<name>.<subdomain>.workers.dev)
 * must be set as the INTERVALS_PROXY_URL repository variable in GitHub so
 * that pages.yml injects it into docs/app/index.html.
 */

// Browser origins (real web origins) that are allowed to read responses
// through the browser's CORS policy.  Requests from these origins receive
// Access-Control-Allow-Origin headers echoing the request Origin.
const ALLOWED_ORIGINS = [
  'https://maximumtrainer.github.io',
  'http://localhost:8080',
  'http://localhost:5500',
  'http://127.0.0.1:8080',
];

// Native desktop clients (Qt Widgets build) are not browsers and are not
// subject to CORS.  They identify themselves by sending an X-MT-Client
// header instead of an Origin.  Accepted values are listed here so the
// worker can distinguish "our own desktop traffic" from random server-side
// callers without abusing the Origin header.  This is not a security
// boundary — any HTTP client can set the header — it just keeps the
// allow-list honest about which clients are expected.  Must match the C++
// constants INTERVALS_PROXY_CLIENT_HEADER /
// INTERVALS_PROXY_DESKTOP_CLIENT_VALUE in src/persistence/db/environnement.h.
const CLIENT_HEADER = 'x-mt-client';
const ALLOWED_CLIENTS = ['desktop'];

const ICU_BASE = 'https://intervals.icu';

// Headers forwarded to intervals.icu — only those the API actually needs.
const FORWARD_HEADERS = [
  'authorization',
  'content-type',
  'accept',
  'accept-encoding',
  'accept-language',
];

function corsHeaders(origin) {
  return {
    'Access-Control-Allow-Origin': origin,
    'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, PATCH, OPTIONS',
    'Access-Control-Allow-Headers': 'authorization, content-type, accept',
    'Access-Control-Max-Age': '86400',
    'Vary': 'Origin, Access-Control-Request-Method, Access-Control-Request-Headers',
  };
}

function forbidden() {
  return new Response('Forbidden', { status: 403 });
}

export default {
  async fetch(request, _env, _ctx) {
    const origin = request.headers.get('Origin') || '';
    const client = (request.headers.get(CLIENT_HEADER) || '').toLowerCase();
    const hasOrigin = origin !== '';
    const isBrowser = hasOrigin && ALLOWED_ORIGINS.includes(origin);
    const isDesktop = !hasOrigin && ALLOWED_CLIENTS.includes(client);

    // Reject anything that's neither a known browser origin nor a known
    // desktop client to prevent open-proxy abuse.
    if (!isBrowser && !isDesktop) {
      return forbidden();
    }

    // CORS preflight (browsers only — desktop clients don't preflight).
    if (request.method === 'OPTIONS') {
      const reqHeaders = request.headers.get('Access-Control-Request-Headers') ||
                         'authorization, content-type, accept';
      return new Response(null, {
        status: 204,
        headers: {
          ...corsHeaders(origin),
          'Access-Control-Allow-Headers': reqHeaders,
        },
      });
    }

    const url = new URL(request.url);

    if (!url.pathname.startsWith('/proxy/')) {
      return new Response('Not Found', {
        status: 404,
        headers: isBrowser ? corsHeaders(origin) : {},
      });
    }

    // Strip /proxy prefix, keep the rest of the path and query string.
    const targetPath = url.pathname.slice('/proxy'.length);
    const targetUrl  = ICU_BASE + targetPath + url.search;

    // Build a clean forwarded request with only the necessary headers.
    // The X-MT-Client marker is intentionally NOT forwarded upstream — it
    // is purely a worker-side allow-list signal.
    const forwardHeaders = new Headers();
    for (const name of FORWARD_HEADERS) {
      const value = request.headers.get(name);
      if (value) forwardHeaders.set(name, value);
    }

    const isBodyMethod = !['GET', 'HEAD'].includes(request.method);
    const proxyReq = new Request(targetUrl, {
      method:  request.method,
      headers: forwardHeaders,
      body:    isBodyMethod ? await request.arrayBuffer() : undefined,
      redirect: 'follow',
    });

    const response = await fetch(proxyReq);

    // Merge CORS headers into the upstream response headers (browsers only;
    // desktop clients ignore CORS and don't need them).
    const responseHeaders = new Headers(response.headers);
    if (isBrowser) {
      for (const [k, v] of Object.entries(corsHeaders(origin))) {
        responseHeaders.set(k, v);
      }
    }
    // Prevent Cloudflare or the browser from caching authenticated responses.
    responseHeaders.set('Cache-Control', 'no-store');

    return new Response(response.body, {
      status:     response.status,
      statusText: response.statusText,
      headers:    responseHeaders,
    });
  },
};
