// persona-core.js — Shared WebSocket + PWA core for all persona pages
// Loaded by all persona HTML files via <script src="/persona-core.js"></script>
(function(global) {
'use strict';

var ws = null;
var statusCbs = [];
var helloCbs = [];
var reconnDelay = 2000;
var maxReconnDelay = 10000;
var curDelay = reconnDelay;
var connState = 'disconnected'; // 'connecting','connected','disconnected'
var reconnTimer = null;
var hostname = '';
var firstStatus = true;

function wsConnect() {
    if (ws && (ws.readyState === 0 || ws.readyState === 1)) return;
    connState = 'connecting';
    updateConnUI();
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = function() {
        connState = 'connected';
        curDelay = reconnDelay;
        firstStatus = true;
        updateConnUI();
    };
    ws.onmessage = function(e) {
        try {
            var d = JSON.parse(e.data);
            if (d.t === 'hello') {
                hostname = d.hostname || '';
                helloCbs.forEach(function(cb) { cb(d); });
            } else if (d.t === 'status') {
                statusCbs.forEach(function(cb) { cb(d, firstStatus); });
                firstStatus = false;
            }
        } catch(x) {}
    };
    ws.onclose = function() {
        ws = null;
        connState = 'disconnected';
        updateConnUI();
        if (reconnTimer) clearTimeout(reconnTimer);
        reconnTimer = setTimeout(wsConnect, curDelay);
        curDelay = Math.min(curDelay * 1.5, maxReconnDelay);
    };
    ws.onerror = function() {};
}

function wsSend(obj) {
    if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
}

function onStatus(cb) { statusCbs.push(cb); }
function onHello(cb) { helloCbs.push(cb); }

function updateConnUI() {
    var b = document.getElementById('offlineBanner');
    if (!b) return;
    if (connState === 'connected') {
        b.style.display = 'none';
    } else {
        b.style.display = 'block';
        b.textContent = connState === 'connecting' ? 'Povezovanje...' : 'Ni povezave \u2014 ponovni poskus...';
    }
}

function getState() { return connState; }
function getHostname() { return hostname; }

// Range slider fill utility
function sfill(el) {
    if (!el) return;
    var pct = ((+el.value - +el.min) / (+el.max - +el.min)) * 100;
    el.style.background = 'linear-gradient(to right,var(--accent,#0af) 0%,var(--accent,#0af) ' + pct + '%,#333 ' + pct + '%,#333 100%)';
}

// Persona config loader
function loadConfig(cb) {
    fetch('/api/persona-cfg')
        .then(function(r) { return r.json(); })
        .then(cb)
        .catch(function() { cb({}); });
}

// Scene list loader
function loadScenes(cb) {
    fetch('/api/scenes')
        .then(function(r) { return r.json(); })
        .then(function(d) { cb(d.scenes || []); })
        .catch(function() { cb([]); });
}

// Cue list loader
function loadCues(cb) {
    fetch('/api/cuelist')
        .then(function(r) { return r.json(); })
        .then(function(d) { cb(d.cues || []); })
        .catch(function() { cb([]); });
}

// Detect standalone PWA mode
function isStandalone() {
    return window.matchMedia('(display-mode: standalone)').matches ||
           window.navigator.standalone === true ||
           !!document.fullscreenElement;
}

// Fullscreen API fallback za bližnjice (ko WebAPK ni na voljo)
function goFullscreen() {
    var el = document.documentElement;
    var rq = el.requestFullscreen || el.webkitRequestFullscreen || el.msRequestFullscreen;
    if (rq) rq.call(el).catch(function(){});
}

function exitFullscreen() {
    var ex = document.exitFullscreen || document.webkitExitFullscreen || document.msExitFullscreen;
    if (ex && document.fullscreenElement) ex.call(document).catch(function(){});
}

// Prikaži fullscreen gumb če ni standalone način
function initFullscreenHint() {
    if (isStandalone()) return;
    // Počakaj da se stran naloži
    var btn = document.createElement('button');
    btn.id = 'fsBtn';
    btn.innerHTML = '&#x26F6;'; // fullscreen unicode
    btn.title = 'Celozaslonski način';
    btn.style.cssText = 'position:fixed;bottom:16px;right:16px;z-index:9999;width:48px;height:48px;' +
        'border-radius:50%;border:none;background:#0af;color:#000;font-size:22px;cursor:pointer;' +
        'box-shadow:0 2px 8px rgba(0,0,0,.5);display:flex;align-items:center;justify-content:center;';
    btn.onclick = function() {
        goFullscreen();
        btn.style.display = 'none';
    };
    document.body.appendChild(btn);
    // Skrij gumb ko je fullscreen aktiven
    document.addEventListener('fullscreenchange', function() {
        btn.style.display = document.fullscreenElement ? 'none' : 'flex';
    });
}

// Zaženi fullscreen hint po DOMContentLoaded
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initFullscreenHint);
} else {
    initFullscreenHint();
}

// Simple toast message
function showMsg(msg) {
    var t = document.getElementById('toast');
    if (!t) return;
    t.textContent = msg;
    t.style.opacity = '1';
    setTimeout(function() { t.style.opacity = '0'; }, 2000);
}

// Service Worker registracija
if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('/sw.js', {scope: '/'})
        .then(function(reg) { console.log('[SW] Registered'); })
        .catch(function(e) { console.log('[SW] Error:', e); });
}

// Export
global.PersonaCore = {
    connect: wsConnect,
    send: wsSend,
    onStatus: onStatus,
    onHello: onHello,
    getState: getState,
    getHostname: getHostname,
    sfill: sfill,
    loadConfig: loadConfig,
    loadScenes: loadScenes,
    loadCues: loadCues,
    isStandalone: isStandalone,
    showMsg: showMsg
};

})(window);
