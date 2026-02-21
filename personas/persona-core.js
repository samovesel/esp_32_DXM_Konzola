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
           window.navigator.standalone === true;
}

// Simple toast message
function showMsg(msg) {
    var t = document.getElementById('toast');
    if (!t) return;
    t.textContent = msg;
    t.style.opacity = '1';
    setTimeout(function() { t.style.opacity = '0'; }, 2000);
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
