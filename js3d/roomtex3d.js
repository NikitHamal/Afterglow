// Afterglow 3D — module: roomtex3d (procedural textures + mesh helpers for room3d)
// Loaded by 3d.html before room3d.js. Everything is generated on canvases at boot:
// no image assets, deterministic seeds so the room looks identical every load.
'use strict';

/* ---------- small helpers ---------- */
function _rng3(seed) {
  let s = seed | 0 || 1;
  return () => (s = (s * 16807) % 2147483647) / 2147483647;
}
function _canvas3(w, h) {
  const cv = document.createElement('canvas');
  cv.width = w; cv.height = h;
  return [cv, cv.getContext('2d')];
}
function _tex3(cv, rx, ry, srgb) {
  const t = new THREE.CanvasTexture(cv);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(rx || 1, ry || 1);
  if (srgb !== false) t.encoding = THREE.sRGBEncoding;
  t.anisotropy = renderer3d ? Math.min(8, renderer3d.capabilities.getMaxAnisotropy()) : 1;
  return t;
}
function _mat3(opts) {
  const m = new THREE.MeshPhysicalMaterial(Object.assign({ roughness: 0.8, metalness: 0 }, opts));
  return m;
}
function _shadow3(m, cast, recv) { m.castShadow = cast !== false; m.receiveShadow = recv !== false; return m; }

/* box with genuinely rounded edges: push every vertex of a finely segmented
   box onto a rounded shell around an inner core box */
function roundedBox3(w, h, d, r, seg) {
  seg = seg || 6;
  const g = new THREE.BoxGeometry(w, h, d, seg * 2, seg * 2, seg * 2);
  const p = g.attributes.position, v = new THREE.Vector3(), c = new THREE.Vector3();
  const hx = w / 2 - r, hy = h / 2 - r, hz = d / 2 - r;
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i);
    c.set(clamp(v.x, -hx, hx), clamp(v.y, -hy, hy), clamp(v.z, -hz, hz));
    v.sub(c);
    if (v.lengthSq() > 1e-12) v.normalize().multiplyScalar(r);
    v.add(c);
    p.setXYZ(i, v.x, v.y, v.z);
  }
  g.computeVertexNormals();
  return g;
}

/* ============================================================
   PROCEDURAL TEXTURES
   ============================================================ */
function woodFloorTex3() {
  const S = 1024, [cv, c] = _canvas3(S, S), rnd = _rng3(77);
  const planks = 8, pw = S / planks;
  for (let i = 0; i < planks; i++) {
    let y0 = 0;
    const off = rnd() * S;
    while (y0 < S + 400) {
      const len = 380 + rnd() * 520;
      const tone = 0.75 + rnd() * 0.5;
      const r = 62 * tone, g = 36 * tone, b = 24 * tone;
      const yy = (y0 + off) % S - (off > 0 ? 0 : 0);
      for (let k = -1; k <= 0; k++) {
        const top = yy + k * S;
        c.fillStyle = `rgb(${r | 0},${g | 0},${b | 0})`;
        c.fillRect(i * pw, top, pw, len);
        // grain: long wavy streaks
        for (let s = 0; s < 34; s++) {
          const gx = i * pw + rnd() * pw, a = 0.05 + rnd() * 0.12;
          c.strokeStyle = rnd() < 0.5 ? `rgba(20,9,5,${a})` : `rgba(120,74,44,${a * 0.8})`;
          c.lineWidth = 0.6 + rnd() * 1.8;
          c.beginPath();
          c.moveTo(gx, top);
          for (let q = 0; q <= len; q += 24) c.lineTo(gx + Math.sin(q * 0.013 + s) * 3 + Math.sin(q * 0.05) * 0.8, top + q);
          c.stroke();
        }
        // a knot now and then
        if (rnd() < 0.35) {
          const kx = i * pw + pw * (0.25 + rnd() * 0.5), ky = top + rnd() * len;
          const kg = c.createRadialGradient(kx, ky, 0, kx, ky, 9 + rnd() * 8);
          kg.addColorStop(0, 'rgba(18,8,4,0.8)'); kg.addColorStop(1, 'rgba(18,8,4,0)');
          c.fillStyle = kg; c.fillRect(kx - 20, ky - 20, 40, 40);
        }
        // butt joint
        c.fillStyle = 'rgba(8,3,2,0.85)';
        c.fillRect(i * pw, top + len - 2, pw, 2);
      }
      y0 += len;
    }
    // plank seam + bevel highlight
    c.fillStyle = 'rgba(6,2,1,0.9)'; c.fillRect(i * pw, 0, 3, S);
    c.fillStyle = 'rgba(160,110,70,0.10)'; c.fillRect(i * pw + 3, 0, 2, S);
  }
  return _tex3(cv, 3, 3);
}

function wallpaperTex3() {
  const S = 512, [cv, c] = _canvas3(S, S), rnd = _rng3(9);
  c.fillStyle = '#3a1a26'; c.fillRect(0, 0, S, S);
  // plaster mottling
  for (let i = 0; i < 900; i++) {
    const x = rnd() * S, y = rnd() * S, r = 6 + rnd() * 40;
    const g = c.createRadialGradient(x, y, 0, x, y, r);
    const a = 0.025 + rnd() * 0.03, light = rnd() < 0.5;
    g.addColorStop(0, light ? `rgba(110,60,74,${a})` : `rgba(10,3,8,${a})`);
    g.addColorStop(1, 'rgba(0,0,0,0)');
    c.fillStyle = g; c.fillRect(x - r, y - r, r * 2, r * 2);
  }
  // damask motif: tone-on-tone, drawn twice per tile on a half-drop
  const motif = (cx, cy, s) => {
    c.save(); c.translate(cx, cy); c.scale(s, s);
    c.strokeStyle = 'rgba(150,84,96,0.16)'; c.fillStyle = 'rgba(150,84,96,0.09)'; c.lineWidth = 2.2;
    for (const m of [1, -1]) {
      c.beginPath();
      c.moveTo(0, -70);
      c.bezierCurveTo(m * 40, -60, m * 52, -18, m * 22, 0);
      c.bezierCurveTo(m * 52, 18, m * 40, 60, 0, 70);
      c.stroke();
      c.beginPath();
      c.moveTo(0, -38); c.bezierCurveTo(m * 26, -30, m * 26, 30, 0, 38); c.fill();
      for (const k of [-1, 1]) {
        c.beginPath(); c.arc(m * 46, k * 46, 9, 0, TAU); c.stroke();
        c.beginPath(); c.moveTo(m * 22, k * 4); c.quadraticCurveTo(m * 60, k * 10, m * 46, k * 37); c.stroke();
      }
    }
    c.beginPath(); c.arc(0, 0, 7, 0, TAU); c.fill();
    c.restore();
  };
  for (const [x, y] of [[128, 128], [384, 384], [384, -128], [128, 640], [-128, 384], [640, 384]]) motif(x, y, 1.15);
  return _tex3(cv, 1, 1);
}

function wainscotTex3() {
  const S = 512, [cv, c] = _canvas3(S, S / 2);
  c.fillStyle = '#1d0c12'; c.fillRect(0, 0, S, S / 2);
  // two raised panels per tile with bevel light/shadow
  for (let i = 0; i < 2; i++) {
    const x = 26 + i * 256, y = 30, w = 204, h = 196;
    c.fillStyle = '#25101a'; c.fillRect(x, y, w, h);
    c.fillStyle = 'rgba(255,200,180,0.07)'; c.fillRect(x, y, w, 4); c.fillRect(x, y, 4, h);
    c.fillStyle = 'rgba(0,0,0,0.45)'; c.fillRect(x, y + h - 4, w, 4); c.fillRect(x + w - 4, y, 4, h);
    c.strokeStyle = 'rgba(0,0,0,0.35)'; c.lineWidth = 2; c.strokeRect(x + 18, y + 18, w - 36, h - 36);
    c.strokeStyle = 'rgba(255,190,170,0.05)'; c.strokeRect(x + 21, y + 21, w - 36, h - 36);
  }
  return _tex3(cv, 1, 1);
}

function weaveTex3(base, seed) {
  const S = 256, [cv, c] = _canvas3(S, S), rnd = _rng3(seed || 5);
  c.fillStyle = base; c.fillRect(0, 0, S, S);
  for (let y = 0; y < S; y += 2) {
    c.fillStyle = `rgba(255,255,255,${0.015 + rnd() * 0.025})`; c.fillRect(0, y, S, 1);
  }
  for (let x = 0; x < S; x += 2) {
    c.fillStyle = `rgba(0,0,0,${0.02 + rnd() * 0.03})`; c.fillRect(x, 0, 1, S);
  }
  for (let i = 0; i < 300; i++) {
    c.fillStyle = `rgba(${rnd() < 0.5 ? '255,255,255' : '0,0,0'},${rnd() * 0.04})`;
    c.fillRect(rnd() * S, rnd() * S, 1 + rnd() * 6, 1);
  }
  return _tex3(cv, 6, 6);
}

function rugTex3() {
  const W2 = 1024, H2 = 1280, [cv, c] = _canvas3(W2, H2), rnd = _rng3(21);
  c.fillStyle = '#4a1420'; c.fillRect(0, 0, W2, H2);
  // field medallion + borders, Persian-ish
  const band = (inset, col, w) => { c.strokeStyle = col; c.lineWidth = w; c.strokeRect(inset, inset, W2 - inset * 2, H2 - inset * 2); };
  band(30, '#1c0a10', 46); band(70, '#b0804a', 6); band(100, '#2a1a3a', 34); band(126, '#b0804a', 4);
  for (let i = 0; i < 46; i++) {      // border lozenges
    const t = i / 46, per = 2 * (W2 + H2 - 400);
    let d = t * per, x, y;
    if (d < W2 - 200) { x = 100 + d; y = 100; } else if ((d -= W2 - 200) < H2 - 200) { x = W2 - 100; y = 100 + d; }
    else if ((d -= H2 - 200) < W2 - 200) { x = W2 - 100 - d; y = H2 - 100; } else { d -= W2 - 200; x = 100; y = H2 - 100 - d; }
    c.fillStyle = i % 2 ? '#c99a5e' : '#8a3040';
    c.beginPath(); c.moveTo(x, y - 11); c.lineTo(x + 11, y); c.lineTo(x, y + 11); c.lineTo(x - 11, y); c.fill();
  }
  c.save(); c.translate(W2 / 2, H2 / 2);
  for (let r = 330; r > 30; r -= 42) {
    c.fillStyle = ['#2a1a3a', '#6a2030', '#b0804a', '#3a1424', '#8a3040'][(r / 42 | 0) % 5];
    c.beginPath();
    for (let a = 0; a <= 64; a++) {
      const ang = a / 64 * TAU, rr2 = r * (1 + 0.12 * Math.cos(ang * 8));
      c.lineTo(Math.cos(ang) * rr2 * 0.85, Math.sin(ang) * rr2 * 1.15);
    }
    c.fill();
  }
  c.restore();
  // pile noise + wear
  for (let i = 0; i < 26000; i++) {
    c.fillStyle = `rgba(${rnd() < 0.5 ? '0,0,0' : '255,220,200'},${rnd() * 0.06})`;
    c.fillRect(rnd() * W2, rnd() * H2, 2, 2);
  }
  return _tex3(cv, 1, 1);
}

function nightSkyTex3() {
  const W2 = 512, H2 = 512, [cv, c] = _canvas3(W2, H2), rnd = _rng3(4);
  const g = c.createLinearGradient(0, 0, 0, H2);
  g.addColorStop(0, '#04060f'); g.addColorStop(0.6, '#0b1430'); g.addColorStop(1, '#1d2546');
  c.fillStyle = g; c.fillRect(0, 0, W2, H2);
  for (let i = 0; i < 260; i++) {
    const a = rnd();
    c.fillStyle = `rgba(220,230,255,${a * a})`;
    c.fillRect(rnd() * W2, rnd() * H2 * 0.8, a > 0.9 ? 2 : 1, a > 0.9 ? 2 : 1);
  }
  // moon with halo
  const mx = W2 * 0.62, my = H2 * 0.3;
  const halo = c.createRadialGradient(mx, my, 10, mx, my, 170);
  halo.addColorStop(0, 'rgba(190,210,255,0.55)'); halo.addColorStop(1, 'rgba(190,210,255,0)');
  c.fillStyle = halo; c.fillRect(0, 0, W2, H2);
  c.fillStyle = '#f2f4ff'; c.beginPath(); c.arc(mx, my, 34, 0, TAU); c.fill();
  c.fillStyle = 'rgba(150,160,190,0.25)';
  for (const [dx, dy, r] of [[-10, -6, 8], [8, 10, 6], [12, -12, 4], [-4, 14, 5]]) { c.beginPath(); c.arc(mx + dx, my + dy, r, 0, TAU); c.fill(); }
  // distant rooftops
  c.fillStyle = '#05060c';
  let x = 0;
  while (x < W2) { const w = 30 + rnd() * 60, h = 40 + rnd() * 90; c.fillRect(x, H2 - h, w, h); x += w; }
  c.fillStyle = 'rgba(255,190,110,0.8)';
  for (let i = 0; i < 18; i++) c.fillRect(rnd() * W2, H2 - 20 - rnd() * 70, 3, 4);
  const t = _tex3(cv, 1, 1); t.wrapS = t.wrapT = THREE.ClampToEdgeWrapping;
  return t;
}

function glowSprite3(inner, outer) {
  const S = 128, [cv, c] = _canvas3(S, S);
  const g = c.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
  g.addColorStop(0, inner); g.addColorStop(0.25, outer); g.addColorStop(1, 'rgba(0,0,0,0)');
  c.fillStyle = g; c.fillRect(0, 0, S, S);
  const t = new THREE.CanvasTexture(cv); t.encoding = THREE.sRGBEncoding;
  return t;
}

