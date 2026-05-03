/**
 * WASM Loading Logger Overlay
 *
 * Creates a fixed, bottom-left diagnostic overlay that logs every stage of
 * the WebAssembly/Qt initialisation process and captures runtime errors —
 * without requiring the user to open DevTools.
 *
 * Features:
 *  - Fixed bottom-left overlay (click-through except for the Copy button)
 *  - Timestamps every line as [HH:MM:SS]
 *  - Intercepts WebAssembly.instantiateStreaming / WebAssembly.instantiate
 *  - Monkey-patches console.error, window.onerror, unhandledrejection
 *  - 📋 Copy button that copies the full log text to the clipboard
 */
(function () {
  'use strict';

  /* ── Timestamp helper ──────────────────────────────────────────────────── */
  function ts() {
    var d   = new Date();
    var pad = function (n) { return String(n).padStart(2, '0'); };
    return '[' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds()) + ']';
  }

  /* ── Build overlay DOM ─────────────────────────────────────────────────── */
  var overlay = document.createElement('div');
  overlay.id = 'wasm-log-overlay';
  overlay.setAttribute('aria-label', 'WASM loading log');
  overlay.setAttribute('role', 'log');
  overlay.style.cssText = [
    'position:fixed',
    'bottom:12px',
    'left:12px',
    'z-index:9999',
    'width:min(480px,90vw)',
    'max-height:200px',
    'overflow-y:auto',
    'background:rgba(0,0,0,0.8)',
    'color:#fff',
    'font-family:monospace',
    'font-size:12px',
    'line-height:1.5',
    'padding:8px 40px 8px 10px',  /* right padding leaves room for copy button */
    'border-radius:6px',
    'pointer-events:none',        /* click-through by default */
    'box-sizing:border-box',
  ].join(';');

  /* Copy button — pointer-events:auto so it remains clickable */
  var copyBtn = document.createElement('button');
  copyBtn.id = 'wasm-log-copy-btn';
  copyBtn.textContent = '📋';
  copyBtn.title = 'Copy log to clipboard';
  copyBtn.setAttribute('aria-label', 'Copy log to clipboard');
  copyBtn.style.cssText = [
    'position:sticky',
    'float:right',
    'top:0',
    'background:rgba(255,255,255,0.15)',
    'border:none',
    'border-radius:4px',
    'color:#fff',
    'font-size:14px',
    'cursor:pointer',
    'padding:2px 6px',
    'pointer-events:auto',
    'line-height:1',
    'margin-left:4px',
  ].join(';');

  copyBtn.addEventListener('click', function () {
    var text = logLines.map(function (l) { return l.textContent; }).join('\n');
    if (!navigator.clipboard) { return; }
    navigator.clipboard.writeText(text).then(function () {
      var orig = copyBtn.textContent;
      copyBtn.textContent = 'Copied!';
      setTimeout(function () { copyBtn.textContent = orig; }, 1500);
    }).catch(function () {});
  });

  var logLines = [];
  var logContent = document.createElement('div');

  overlay.appendChild(copyBtn);
  overlay.appendChild(logContent);

  /* Attach as soon as <body> is available */
  function attach() { document.body.appendChild(overlay); }
  if (document.body) {
    attach();
  } else {
    document.addEventListener('DOMContentLoaded', attach);
  }

  /* ── Core append function ──────────────────────────────────────────────── */
  function appendLog(msg) {
    var line = document.createElement('div');
    line.textContent = ts() + ' ' + msg;
    logContent.appendChild(line);
    logLines.push(line);
    /* Keep scroll pinned to the bottom */
    overlay.scrollTop = overlay.scrollHeight;
  }

  /* Expose publicly so index.html can call window._wasmLogger.log(msg) */
  window._wasmLogger = { log: appendLog };

  /* ── WebAssembly.instantiateStreaming hook ──────────────────────────────── */
  if (typeof WebAssembly !== 'undefined' && WebAssembly.instantiateStreaming) {
    var _origStreaming = WebAssembly.instantiateStreaming;
    WebAssembly.instantiateStreaming = function (source, importObject) {
      appendLog('Fetching WASM…');
      return _origStreaming.call(WebAssembly, source, importObject)
        .then(function (result) {
          appendLog('Initialized Successfully');
          return result;
        })
        .catch(function (err) {
          appendLog('ERROR: ' + (err && err.message ? err.message : String(err)));
          throw err;
        });
    };
  }

  /* ── WebAssembly.instantiate hook ──────────────────────────────────────── */
  if (typeof WebAssembly !== 'undefined' && WebAssembly.instantiate) {
    var _origInstantiate = WebAssembly.instantiate;
    WebAssembly.instantiate = function (source, importObject) {
      /* source is an ArrayBuffer when called directly (already fetched) */
      var isBuffer = (source instanceof ArrayBuffer) ||
                     (typeof SharedArrayBuffer !== 'undefined' && source instanceof SharedArrayBuffer);
      if (isBuffer) {
        appendLog('Compiling WASM…');
      } else {
        appendLog('Fetching WASM…');
      }
      return _origInstantiate.call(WebAssembly, source, importObject)
        .then(function (result) {
          appendLog('Initialized Successfully');
          return result;
        })
        .catch(function (err) {
          appendLog('ERROR: ' + (err && err.message ? err.message : String(err)));
          throw err;
        });
    };
  }

  /* ── console.error patch ───────────────────────────────────────────────── */
  // Qt WASM routes qWarning() to console.error, tagging messages with '] [WARN] ['.
  // Classify these as WARN (not ERROR) so the overlay is not misleadingly full of
  // ERROR lines when the app has loaded successfully.  Fatal JS/WASM errors do not
  // carry the Qt severity tag and are still classified as ERROR.
  var _origConsoleError = console.error.bind(console);
  console.error = function () {
    var parts = Array.prototype.slice.call(arguments);
    var msg = parts.map(function (a) {
      return (a instanceof Error) ? a.message : String(a);
    }).join(' ');
    var prefix = (msg.indexOf('] [WARN] [') !== -1) ? 'WARN' : 'ERROR';
    appendLog(prefix + ': ' + msg);
    _origConsoleError.apply(console, arguments);
  };

  /* ── window.onerror patch ──────────────────────────────────────────────── */
  var _prevOnError = window.onerror;
  window.onerror = function (msg, src, line, col, err) {
    var location = src ? ' (' + src + ':' + line + ')' : '';
    appendLog('ERROR: ' + msg + location);
    if (typeof _prevOnError === 'function') { return _prevOnError.apply(this, arguments); }
    return false;
  };

  /* ── unhandledrejection ────────────────────────────────────────────────── */
  window.addEventListener('unhandledrejection', function (ev) {
    var reason = (ev.reason instanceof Error) ? ev.reason.message : String(ev.reason);
    appendLog('ERROR: Unhandled rejection: ' + reason);
  });

})();
