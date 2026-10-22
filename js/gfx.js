// Afterglow — module: gfx (loaded by index.html)
// Ultra-Realistic Procedural Rendering Engine
'use strict';

/* ============================================================
   PHYSICS & KINEMATICS SHEAR
   ============================================================ */
function hp(x, y, f) {
  const d = G.depth || 0;
  return [x - 98 * d * f, y + 26 * d * f];
}

function sp(x, y, f) {
  const d = (G.depth || 0) * 0.14 + (G.impact || 0) * 0.06;
  return [x - 98 * d * f, y + 26 * d * f];
}

function chaos(x) {
  return Math.sin((G.t || 0) * 37 + x * 13) * (G.shake || 0);
}

/* ============================================================
   GRADIENT CACHE
   CanvasGradient objects are immutable once built, so identical
   (transform, geometry, stops) requests can safely share one.
   Gradient coordinates are interpreted in the CTM at paint time,
   so the current transform is part of the key. Exact keys keep the
   reuse bit-for-bit identical to the uncached result.
   ============================================================ */
const _gcache = new Map();
const _GCAP = 4096;
function _gkey(kind, nums, stops) {
  const m = X.getTransform();
  let k = kind + m.a + '|' + m.b + '|' + m.c + '|' + m.d + '|' + m.e + '|' + m.f + '|';
  for (let i = 0; i < nums.length; i++) k += nums[i] + '|';
  for (let i = 0; i < stops.length; i++) k += stops[i][0] + ':' + stops[i][1] + '|';
  return k;
}
function _gget(k, make) {
  let g = _gcache.get(k);
  if (g === undefined) {
    if (_gcache.size >= _GCAP) _gcache.clear();
    g = make();
    _gcache.set(k, g);
  }
  return g;
}
function gLinear(x0, y0, x1, y1, stops) {
  return _gget(_gkey('L', [x0, y0, x1, y1], stops), () => {
    const g = X.createLinearGradient(x0, y0, x1, y1);
    for (let i = 0; i < stops.length; i++) g.addColorStop(stops[i][0], stops[i][1]);
    return g;
  });
}
function gRadial(x0, y0, r0, x1, y1, r1, stops) {
  return _gget(_gkey('R', [x0, y0, r0, x1, y1, r1], stops), () => {
    const g = X.createRadialGradient(x0, y0, r0, x1, y1, r1);
    for (let i = 0; i < stops.length; i++) g.addColorStop(stops[i][0], stops[i][1]);
    return g;
  });
}

/* ============================================================
   ADVANCED SHADING & PROCEDURAL PRIMITIVES
   ============================================================ */
function sg(y0, y1, c1, c2) {
  const ya = Number.isFinite(y0) ? y0 : 0;
  const yb = Number.isFinite(y1) ? y1 : 100;
  return gLinear(0, ya, 0, yb, [[0, c1], [1, c2]]);
}

function shade(x, y, rx, ry, col, rot = 0, lc) {
  const cx = Number.isFinite(x) ? x : 0, cy = Number.isFinite(y) ? y : 0;
  const ax = Math.max(1, Number.isFinite(rx) ? Math.abs(rx) : 10);
  const ay = Math.max(1, Number.isFinite(ry) ? Math.abs(ry) : 10);
  // Draw the gradient in a space scaled to the ellipse, so it reaches full
  // transparency exactly at the rim. A circular gradient inside an elliptical
  // clip leaves a hard edge wherever the minor axis is much shorter than the
  // major one (elongated AO pools used to show up as pasted-on ovals).
  X.save();
  X.translate(cx, cy);
  if (rot) X.rotate(rot);
  X.scale(ax, ay);
  const g = gRadial(0, 0, 0, 0, 0, 1, [[0, lc ? lc : col], [1, 'rgba(0,0,0,0)']]);
  X.fillStyle = g;
  X.beginPath();
  X.arc(0, 0, 1, 0, TAU);
  X.fill();
  X.restore();
}

/**
 * Volumetric cylindrical capsule with directional lighting,
 * subsurface scattering (SSS) terminator red-shift, and ambient occlusion.
 */
function capsule(a, b, r1, r2, fill, bob = 0) {
  if (!a || !b || !isFinite(a[0]) || !isFinite(a[1]) || !isFinite(b[0]) || !isFinite(b[1])) return;
  const dx = b[0] - a[0], dy = b[1] - a[1];
  const len = Math.hypot(dx, dy);
  if (len < 0.001) return;
  const ang = Math.atan2(dy, dx);
  const perp = ang + Math.PI / 2;

  X.beginPath();
  X.arc(a[0], a[1] + bob, Math.max(0.1, r1), ang + Math.PI / 2, ang + Math.PI * 1.5);
  X.arc(b[0], b[1] + bob, Math.max(0.1, r2), ang - Math.PI / 2, ang + Math.PI / 2);
  X.closePath();

  // Base fill
  if (fill instanceof CanvasGradient || fill instanceof CanvasPattern) {
    X.fillStyle = fill;
    X.fill();
  } else {
    const g = X.createLinearGradient(a[0], a[1], b[0], b[1]);
    if (typeof fill === 'string') { g.addColorStop(0, fill); g.addColorStop(0.5, fill); g.addColorStop(1, fill); }
    else if (Array.isArray(fill)) { g.addColorStop(0, fill[0]); g.addColorStop(1, fill[1]); }
    else { X.fillStyle = fill; X.fill(); return; }
    X.fillStyle = g;
    X.fill();
  }

  // Cylindrical 3D cross-shading: Key light from upper-left lamp (127, 400)
  const midX = (a[0] + b[0]) / 2, midY = (a[1] + b[1]) / 2 + bob;
  const nx = Math.cos(perp), ny = Math.sin(perp);
  const avgR = (r1 + r2) / 2;

  // Subsurface Scattering (warm peach/red glow right at shadow terminator)
  X.save();
  X.globalCompositeOperation = 'soft-light';
  const sssG = X.createLinearGradient(midX - nx * avgR, midY - ny * avgR, midX + nx * avgR, midY + ny * avgR);
  sssG.addColorStop(0, 'rgba(255,140,110,0.42)');
  sssG.addColorStop(0.35, 'rgba(255,220,190,0.30)');
  sssG.addColorStop(0.7, 'rgba(180,60,40,0.25)');
  sssG.addColorStop(1, 'rgba(80,20,15,0.40)');
  X.fillStyle = sssG;
  X.fill();
  X.restore();

  // Diffuse highlight ridge along light-facing side
  X.save();
  X.globalCompositeOperation = 'screen';
  const hlG = X.createLinearGradient(midX - nx * avgR, midY - ny * avgR, midX + nx * avgR, midY + ny * avgR);
  hlG.addColorStop(0, 'rgba(255,245,235,0.32)');
  hlG.addColorStop(0.38, 'rgba(255,230,210,0.08)');
  hlG.addColorStop(0.7, 'rgba(0,0,0,0)');
  hlG.addColorStop(1, 'rgba(0,0,0,0)');
  X.fillStyle = hlG;
  X.fill();
  X.restore();

  // Ambient Occlusion / Underside shadow
  X.save();
  X.globalCompositeOperation = 'multiply';
  const aoG = X.createLinearGradient(midX - nx * avgR, midY - ny * avgR, midX + nx * avgR, midY + ny * avgR);
  aoG.addColorStop(0, 'rgba(0,0,0,0)');
  aoG.addColorStop(0.5, 'rgba(0,0,0,0)');
  aoG.addColorStop(0.85, 'rgba(40,15,18,0.28)');
  aoG.addColorStop(1, 'rgba(25,8,10,0.48)');
  X.fillStyle = aoG;
  X.fill();
  X.restore();
}

function heartPath(x, y, s) {
  X.beginPath();
  X.moveTo(x, y + s * 0.95);
  X.bezierCurveTo(x - s * 1.35, y + s * 0.12, x - s * 0.85, y - s * 0.95, x, y - s * 0.32);
  X.bezierCurveTo(x + s * 0.85, y - s * 0.95, x + s * 1.35, y + s * 0.12, x, y + s * 0.95);
  X.closePath();
}

/* Room, bed, lighting and the screen-space post pass live in js/atmos.js. */

/* ============================================================
   FLUIDS, REFRACTION, SWEAT & CLIMAX PARTICLES
   ============================================================ */
function drawFluids() {
  const t = G.t || 0;

  // Climax drips with gravity stretching, surface tension, and internal meniscus gleams
  (G.drips || []).forEach(d => {
    const ox = d.ox ?? d.x, oy = d.oy ?? 505;
    const pts = [
      [ox, oy],
      [ox - 7, oy + 22],
      [ox - 12, oy + 46],
      [ox - 16, oy + 68]
    ];
    let p = d.p * (pts.length - 1);
    let i = Math.min(p | 0, pts.length - 2);
    let f = p - i;
    const y = lerp(pts[i][1], pts[i + 1][1], f);
    const x = lerp(pts[i][0], pts[i + 1][0], f) + Math.sin(d.p * 9.5 + (d.j || 0)) * 1.8;

    // Outer viscous fluid body
    X.fillStyle = 'rgba(255,248,244,0.78)';
    X.beginPath();
    X.ellipse(x, y, 2.6, 4.4, 0, 0, TAU);
    X.fill();

    // Trailing connecting strand
    X.strokeStyle = 'rgba(255,248,244,0.42)';
    X.lineWidth = 1.4;
    X.beginPath();
    X.moveTo(x, y - 4);
    X.quadraticCurveTo(x + 1.5, y - 10, x - 1, y - 16);
    X.stroke();

    // High specular pinpoint catchlight
    X.fillStyle = 'rgba(255,255,255,0.95)';
    X.beginPath();
    X.arc(x - 0.7, y - 1.2, 1.1, 0, TAU);
    X.fill();
  });

  // Climax ejaculatory jets: ballistic ropes with viscous head and fading tail
  (G.jets || []).forEach(j => {
    if (j.view !== 'side') return;
    const spd = Math.hypot(j.vx, j.vy);
    const ang = Math.atan2(j.vy, j.vx);
    const alpha = clamp(j.life * 2.4, 0, 1);

    // Jet droplet head
    X.save();
    X.fillStyle = `rgba(255,250,246,${0.88 * alpha})`;
    X.beginPath();
    X.ellipse(j.x, j.y, 4 + spd * 0.009, 2.4, ang, 0, TAU);
    X.fill();

    // High velocity stretch tail
    const grad = X.createLinearGradient(j.x, j.y, j.x - Math.cos(ang) * 16, j.y - Math.sin(ang) * 16);
    grad.addColorStop(0, `rgba(255,245,240,${0.75 * alpha})`);
    grad.addColorStop(1, 'rgba(255,245,240,0)');
    X.strokeStyle = grad;
    X.lineWidth = 2.6;
    X.beginPath();
    X.moveTo(j.x, j.y);
    X.lineTo(j.x - Math.cos(ang) * 16, j.y - Math.sin(ang) * 16);
    X.stroke();
    X.restore();
  });

  // Wetness glisten specks catching light
  (G.glisten || []).forEach(g => {
    const pulse = 0.5 + 0.5 * Math.sin(t * 8 + g.x);
    X.fillStyle = `rgba(255,252,248,${g.a * 0.65 * pulse})`;
    X.beginPath();
    X.arc(g.x, g.y, 1.6, 0, TAU);
    X.fill();
  });

  // Sweat droplets: refractive bead with bottom shadow and top specular
  (G.sweat || []).forEach(s => {
    const a = 0.55 * s.life;
    X.fillStyle = `rgba(20,5,8,${0.25 * a})`;
    X.beginPath();
    X.ellipse(s.x, s.y + 1, 1.6, 1.1, 0, 0, TAU);
    X.fill();
    X.fillStyle = `rgba(255,255,255,${0.85 * a})`;
    X.beginPath();
    X.arc(s.x - 0.4, s.y - 0.5, 0.9, 0, TAU);
    X.fill();
  });

  // Floating euphoric hearts
  (G.hearts || []).forEach(h => {
    const fade = Math.min(1, h.life);
    X.save();
    X.fillStyle = `rgba(255,100,135,${0.62 * fade})`;
    X.shadowColor = 'rgba(255,80,120,0.5)';
    X.shadowBlur = 8;
    heartPath(h.x + Math.sin(h.ph) * 7, h.y, 7.5 * h.s);
    X.fill();
    X.restore();
  });
}
