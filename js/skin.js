// Afterglow — module: skin (shared airbrushed-skin + anatomy primitives)
// Loaded after gfx.js. Provides organic tapered limbs, form-shaded skin fills,
// hands/feet/breasts/vulva/neck and silky hair ribbons used by side.js & fpv.js.
'use strict';

/* ============================================================
   TONE DERIVATION
   ============================================================ */
function skMix(a, b, t){ return lerpHex(a, b, clamp(t, 0, 1)); }
function skLight(h, t){ return skMix(h, '#fff4ea', t); }
function skDark(h, t){ return skMix(h, '#3a1410', t); }

// Build a 4-stop tone ramp {hi,b,s,d} from a base skin hex.
function skTone(base){
  const d = skDark(base, 0.5);
  return { hi: skLight(base, 0.42), b: base, s: skDark(base, 0.26), d, dk: d };
}
function herT(){ const s = getSkin(); return Object.assign(skTone(s.her), { base: s.her, sh: s.herSh, dk: s.herDk }); }
function himT(){ const s = getSkin(); return Object.assign(skTone(s.him), { base: s.him, sh: s.himSh, dk: s.himSh }); }

// Key light = warm bedside lamp upper-left; cool moon fill from right.
const SK_LIT = [-0.62, -0.78];

/* ============================================================
   LOW-LEVEL SHAPE FILL + FORM SHADING
   ============================================================ */
// Fill a caller-defined closed path with a directional skin gradient.
// lg = [x0,y0,x1,y1] light->shadow axis. Leaves path current for clipping.
function skFillShape(fn, T, lg){
  X.beginPath(); fn();
  const g = X.createLinearGradient(lg[0], lg[1], lg[2], lg[3]);
  g.addColorStop(0, T.hi); g.addColorStop(0.38, T.b);
  g.addColorStop(0.78, T.s); g.addColorStop(1, T.d);
  X.fillStyle = g; X.fill();
}
// Fill a caller-defined closed path with a radial skin gradient (heads, domes).
function skFillRad(fn, T, cx, cy, r0, r1){
  X.beginPath(); fn();
  const g = X.createRadialGradient(cx, cy, r0, cx, cy, r1);
  g.addColorStop(0, T.hi); g.addColorStop(0.45, T.b);
  g.addColorStop(0.82, T.s); g.addColorStop(1, T.d);
  X.fillStyle = g; X.fill();
}
// Run shading callbacks clipped inside a caller-defined path.
function skClipIn(fn, inner){ X.save(); X.beginPath(); fn(); X.clip(); inner(); X.restore(); }
// Soft multiply form-shadow blob.
function fSh(x, y, rx, ry, col, rot){ X.save(); X.globalCompositeOperation = 'multiply'; shade(x, y, rx, ry, col, rot || 0); X.restore(); }
// Soft screen highlight blob.
function fHi(x, y, rx, ry, col, rot){ X.save(); X.globalCompositeOperation = 'screen'; shade(x, y, rx, ry, col, rot || 0); X.restore(); }
// Warm subsurface glow at shadow terminator.
function fSSS(x, y, rx, ry, rot){ X.save(); X.globalCompositeOperation = 'soft-light'; shade(x, y, rx, ry, 'rgba(255,120,90,0.40)', rot || 0); X.restore(); }
// Contact / ambient-occlusion pool.
function fAO(x, y, rx, ry, a, rot){ fSh(x, y, rx, ry, `rgba(30,10,12,${a})`, rot); }
// Delicate silhouette line so forms read against dark bg (hentai linework).
function skLine(fn, T, w, a){ X.save(); X.beginPath(); fn(); X.strokeStyle = `rgba(${hexToRgb(T.dk).join(',')},${a == null ? 0.4 : a})`; X.lineWidth = w || 1.4; X.stroke(); X.restore(); }

/* ============================================================
   ORGANIC TAPERED LIMB (replaces mechanical capsule)
   a,b endpoints · r1,r2 end radii · opt{bow,belly,lit,aoA,aoB,line}
   ============================================================ */
function limbS(a, b, r1, r2, T, opt){
  opt = opt || {};
  if(!a || !b || !isFinite(a[0]) || !isFinite(b[0])) return;
  const dx = b[0] - a[0], dy = b[1] - a[1], len = Math.hypot(dx, dy);
  if(len < 0.001) return;
  const ux = dx / len, uy = dy / len;            // along
  const px = -uy, py = ux;                        // perpendicular
  const bow = opt.bow || 0;
  const mx = (a[0] + b[0]) / 2 + px * bow, my = (a[1] + b[1]) / 2 + py * bow;
  const rm = ((r1 + r2) / 2) * (opt.belly || 1.06);
  const lit = opt.lit || SK_LIT;

  const path = () => {
    X.moveTo(a[0] + px * r1, a[1] + py * r1);
    X.quadraticCurveTo(mx + px * rm, my + py * rm, b[0] + px * r2, b[1] + py * r2);
    X.quadraticCurveTo(b[0] + ux * r2 * 1.32, b[1] + uy * r2 * 1.32, b[0] - px * r2, b[1] - py * r2);
    X.quadraticCurveTo(mx - px * rm, my - py * rm, a[0] - px * r1, a[1] - py * r1);
    X.quadraticCurveTo(a[0] - ux * r1 * 1.32, a[1] - uy * r1 * 1.32, a[0] + px * r1, a[1] + py * r1);
    X.closePath();
  };

  // directional volume: light side -> shadow side across the limb
  const R = Math.max(r1, r2) * 1.15;
  const cx = (a[0] + b[0]) / 2, cy = (a[1] + b[1]) / 2;
  skFillShape(path, T, [cx + lit[0] * R, cy + lit[1] * R, cx - lit[0] * R, cy - lit[1] * R]);

  skClipIn(path, () => {
    // specular crest toward light
    fHi(cx + lit[0] * rm * 0.5, cy + lit[1] * rm * 0.5, len * 0.42, rm * 0.5, 'rgba(255,240,225,0.30)', Math.atan2(uy, ux));
    // warm SSS terminator
    fSSS(cx - lit[0] * rm * 0.35, cy - lit[1] * rm * 0.35, len * 0.4, rm * 0.6, Math.atan2(uy, ux));
    // core shadow on far edge
    fSh(cx - lit[0] * rm * 0.85, cy - lit[1] * rm * 0.85, len * 0.5, rm * 0.62, 'rgba(70,26,22,0.42)', Math.atan2(uy, ux));
    if(opt.aoA) fAO(a[0], a[1], r1 * 1.25, r1 * 1.25, opt.aoA);
    if(opt.aoB) fAO(b[0], b[1], r2 * 1.25, r2 * 1.25, opt.aoB);
  });
  if(opt.line !== false) skLine(path, T, 1.3, 0.32);
}

/* ============================================================
   HAND: palm + four articulated fingers + thumb
   ============================================================ */
function handS(x, y, ang, s, T, opt){
  opt = opt || {};
  const curl = opt.curl == null ? 0.35 : opt.curl;   // 0 flat .. 1 fist
  const spread = opt.spread == null ? 0.30 : opt.spread;
  X.save(); X.translate(x, y); X.rotate(ang); X.scale(s, s);
  // palm: rounded trapezoid
  const palm = () => {
    X.moveTo(-9, -8); X.quadraticCurveTo(0, -11, 9, -8);
    X.quadraticCurveTo(12, 0, 9, 8); X.quadraticCurveTo(0, 12, -9, 8);
    X.quadraticCurveTo(-12, 0, -9, -8); X.closePath();
  };
  skFillShape(palm, T, [-10, -10, 10, 10]);
  skClipIn(palm, () => { fHi(-2, -3, 7, 5, 'rgba(255,240,225,0.30)'); fAO(0, 8, 9, 4, 0.3); });
  // four fingers fanning from knuckle line
  for(let i = 0; i < 4; i++){
    const kx = -7 + i * 4.6, ky = -8;
    const fa = -Math.PI / 2 + (i - 1.5) * spread * 0.5;
    const L = (i === 0 || i === 3) ? 13 : 16;
    const bend = curl * 1.1;
    const j1 = [kx + Math.cos(fa) * L * 0.5, ky + Math.sin(fa) * L * 0.5];
    const j2 = [kx + Math.cos(fa + bend * 0.5) * L, ky + Math.sin(fa + bend * 0.5) * L + curl * 5];
    limbS([kx, ky], j1, 2.6, 2.2, T, { line: false, belly: 1.0 });
    limbS(j1, j2, 2.2, 1.6, T, { line: false, belly: 1.0 });
    X.fillStyle = 'rgba(255,235,225,0.5)';           // nail
    X.beginPath(); X.ellipse(j2[0], j2[1] + 0.4, 1.3, 1.0, fa, 0, TAU); X.fill();
  }
  // thumb opposing
  const ta = Math.PI * 0.78;
  const t1 = [-8 + Math.cos(ta) * 6, 2 + Math.sin(ta) * 6];
  const t2 = [-8 + Math.cos(ta - curl) * 12, 2 + Math.sin(ta - curl) * 12];
  limbS([-8, 2], t1, 3.2, 2.6, T, { line: false });
  limbS(t1, t2, 2.6, 2.0, T, { line: false });
  // knuckle hints
  X.fillStyle = 'rgba(120,60,50,0.20)';
  for(let i = 0; i < 4; i++){ X.beginPath(); X.arc(-7 + i * 4.6, -7.4, 0.9, 0, TAU); X.fill(); }
  skLine(palm, T, 1.1, 0.3);
  X.restore();
}

/* ============================================================
   FOOT: heel, arch, forefoot, toes
   ============================================================ */
function footS(x, y, ang, s, T){
  X.save(); X.translate(x, y); X.rotate(ang); X.scale(s, s);
  const path = () => {
    X.moveTo(-12, -4); X.quadraticCurveTo(-2, -8, 8, -5);
    X.quadraticCurveTo(16, -3, 17, 1); X.quadraticCurveTo(15, 6, 6, 6);
    X.quadraticCurveTo(-6, 7, -12, 4); X.quadraticCurveTo(-15, 0, -12, -4); X.closePath();
  };
  skFillShape(path, T, [-12, -8, 12, 6]);
  skClipIn(path, () => { fHi(2, -3, 9, 3, 'rgba(255,240,225,0.3)'); fAO(-9, 3, 5, 3, 0.3); });
  for(let i = 0; i < 5; i++){
    const tx = 15 - i * 1.2, ty = 2 + i * 1.1;
    X.fillStyle = T.b; X.beginPath(); X.arc(tx, ty, 2.2 - i * 0.3, 0, TAU); X.fill();
    X.fillStyle = 'rgba(255,235,228,0.7)'; X.beginPath(); X.ellipse(tx + 0.3, ty - 0.5, 0.9, 0.6, 0.2, 0, TAU); X.fill();
  }
  skLine(path, T, 1.2, 0.3);
  X.restore();
}

/* ============================================================
   BREAST: integrated teardrop with gravity, areola, specular
   cx,cy = nipple-ish apex anchor · r radius · ang orientation
   ============================================================ */
function breastS(cx, cy, r, ang, T, opt){
  opt = opt || {};
  const ar = clamp((G.ar || 0) / 100, 0, 1);
  const jig = opt.jig || 0;
  X.save(); X.translate(cx, cy + jig); X.rotate(ang);
  const path = () => {
    // teardrop: chest-wall attach (right) sweeping out to apex (left)
    X.moveTo(r * 0.9, -r * 0.85);
    X.bezierCurveTo(-r * 0.2, -r * 1.05, -r * 1.25, -r * 0.5, -r * 1.15, r * 0.15);
    X.bezierCurveTo(-r * 1.05, r * 0.85, -r * 0.2, r * 1.05, r * 0.9, r * 0.8);
    X.bezierCurveTo(r * 1.15, r * 0.3, r * 1.15, -r * 0.3, r * 0.9, -r * 0.85);
    X.closePath();
  };
  // radial form light from upper-outer
  X.beginPath(); path();
  const g = X.createRadialGradient(-r * 0.45, -r * 0.5, r * 0.1, 0, 0, r * 1.5);
  g.addColorStop(0, T.hi); g.addColorStop(0.42, T.b); g.addColorStop(0.8, T.s); g.addColorStop(1, T.d);
  X.fillStyle = g; X.fill();
  skClipIn(path, () => {
    fHi(-r * 0.42, -r * 0.48, r * 0.5, r * 0.36, 'rgba(255,244,232,0.42)', -0.5);   // crest specular
    fSh(r * 0.1, r * 0.85, r * 0.95, r * 0.4, 'rgba(90,35,30,0.5)', 0.12);          // under-breast shadow
    fSSS(-r * 0.9, r * 0.35, r * 0.5, r * 0.4, 0.6);                                // rim SSS
    fAO(r * 0.85, 0, r * 0.4, r * 0.9, 0.35);                                       // chest-wall contact
  });
  // areola + nipple on the apex, squashed by viewing angle
  const aer = r * (0.34 + 0.06 * ar);
  X.save(); X.translate(-r * 0.92, r * 0.02); X.rotate(0.1);
  X.fillStyle = `rgba(${hexToRgb(G.char ? G.char.nippleColor : '#c25f63').join(',')},0.55)`;
  X.beginPath(); X.ellipse(0, 0, aer * 0.62, aer, 0, 0, TAU); X.fill();
  X.fillStyle = G.char ? G.char.nippleColor : '#c25f63';
  X.beginPath(); X.ellipse(-aer * 0.15, 0, aer * 0.3, aer * 0.42 + ar * 1.5, 0, 0, TAU); X.fill();
  X.fillStyle = 'rgba(255,255,255,0.5)';
  X.beginPath(); X.arc(-aer * 0.3, -aer * 0.25, aer * 0.14, 0, TAU); X.fill();
  X.restore();
  skLine(path, T, 1.3, 0.3);
  X.restore();
}

/* ============================================================
   VULVA: soft mons + labia folds (filled, not outline ellipses)
   ============================================================ */
function vulvaS(cx, cy, open, eng, T, opt){
  opt = opt || {};
  const view = opt.view || 'side';   // 'side' | 'front'
  // mons mound
  const mons = () => { X.ellipse(cx, cy - 4, 15, 13, 0, 0, TAU); };
  X.save(); X.beginPath(); mons();
  const mg = X.createRadialGradient(cx - 4, cy - 9, 2, cx, cy - 4, 18);
  mg.addColorStop(0, T.hi); mg.addColorStop(0.55, T.b); mg.addColorStop(1, T.s);
  X.fillStyle = mg; X.fill(); X.restore();

  if(view === 'front'){
    // outer labia as two soft filled folds
    for(const s of [-1, 1]){
      X.beginPath();
      X.moveTo(cx + s * 2, cy - 13);
      X.bezierCurveTo(cx + s * 9, cy - 9, cx + s * 9.5, cy + 4, cx + s * 3, cy + 12);
      X.bezierCurveTo(cx + s * 1.5, cy + 6, cx + s * 1.5, cy - 6, cx + s * 2, cy - 13);
      X.closePath();
      X.fillStyle = T.s; X.fill();
      fSh(cx + s * 5, cy, 3.4, 10, 'rgba(120,45,50,0.45)', 0);
    }
    // inner minora roseate
    X.fillStyle = `rgba(${205 + 30 * eng | 0},${95 + 20 * eng | 0},${110 + 20 * eng | 0},0.9)`;
    X.beginPath(); X.ellipse(cx, cy + 1, 2.6 + open * 0.25, 8 + open * 0.5, 0, 0, TAU); X.fill();
    // introitus
    X.fillStyle = 'rgba(70,20,30,0.9)';
    X.beginPath(); X.ellipse(cx, cy + 3, 2.2, open * 0.6, 0, 0, TAU); X.fill();
    // clitoral hood + glans
    X.fillStyle = T.b; X.beginPath(); X.ellipse(cx, cy - 10, 4, 3.2, 0, Math.PI, 0); X.fill();
    X.fillStyle = `rgba(${215 + 25 * eng | 0},105,118,0.95)`;
    X.beginPath(); X.arc(cx, cy - 9, 1.4 + 1.4 * eng, 0, TAU); X.fill();
  } else {
    // side: single cleft with swollen labia edge
    X.strokeStyle = `rgba(${185 + 25 * eng | 0},105,100,0.8)`; X.lineWidth = 3.4; X.lineCap = 'round';
    X.beginPath(); X.moveTo(cx + 1, cy - 12); X.quadraticCurveTo(cx + 5, cy, cx + 1, cy + 11); X.stroke();
    X.fillStyle = `rgba(${205 + 30 * eng | 0},100,112,0.85)`;
    X.beginPath(); X.ellipse(cx + 2, cy, 2.4, 8 + open * 0.4, 0.1, 0, TAU); X.fill();
    X.fillStyle = 'rgba(70,20,30,0.85)';
    X.beginPath(); X.ellipse(cx + 2.5, cy + 2, 2.0, open * 0.55, 0.1, 0, TAU); X.fill();
    X.fillStyle = `rgba(${215 + 25 * eng | 0},105,118,0.95)`;
    X.beginPath(); X.arc(cx + 1, cy - 10, 1.4 + 1.4 * eng, 0, TAU); X.fill();
  }
  // wet specular
  if((G.ar || 0) > 22){
    X.save(); X.globalCompositeOperation = 'screen';
    X.strokeStyle = `rgba(255,255,255,${0.25 + 0.3 * eng})`; X.lineWidth = 1.8; X.lineCap = 'round';
    X.beginPath(); X.moveTo(cx + (view === 'front' ? 3 : 4), cy - 8);
    X.quadraticCurveTo(cx + (view === 'front' ? 5 : 6), cy, cx + (view === 'front' ? 3 : 4), cy + 8); X.stroke();
    X.restore();
  }
  // pubic hair (kept per customization) — soft dark tresses over mons
  const ph = G.char ? G.char.pubicHair : 'trim';
  if(ph !== 'bare'){
    const n = ph === 'full' ? 9 : 5;
    const hc = skDark(G.char ? G.char.hairColor : '#231318', 0.1);
    X.save(); X.lineCap = 'round';
    for(let i = 0; i < n; i++){
      const a0 = Math.PI * (0.2 + (i / (n - 1)) * 0.6);
      const r0 = 7, r1 = ph === 'full' ? 16 : 12;
      X.strokeStyle = hc; X.globalAlpha = 0.7; X.lineWidth = 1.9 - (i % 3) * 0.4;
      X.beginPath();
      X.moveTo(cx + Math.cos(a0) * r0 * 0.5, cy - 14 + Math.sin(a0) * r0 * 0.3);
      X.quadraticCurveTo(cx + Math.cos(a0) * r1 * 0.8, cy - 14 + Math.sin(a0) * r1 * 0.6,
        cx + Math.cos(a0) * r1, cy - 14 + Math.sin(a0) * r1);
      X.stroke();
    }
    X.restore();
  }
}

/* ============================================================
   NECK + TRAPEZIUS: connects skull to shoulder line
   ============================================================ */
function neckS(top, base, w, T, opt){
  opt = opt || {};
  const dx = base[0] - top[0], dy = base[1] - top[1];
  const px = -dy, py = dx, L = Math.hypot(dx, dy) || 1;
  const nx = px / L, ny = py / L;
  const path = () => {
    X.moveTo(top[0] + nx * w * 0.62, top[1] + ny * w * 0.62);
    // trapezius flare out to shoulder at base
    X.quadraticCurveTo(base[0] + nx * w * 0.7, base[1] + ny * w * 0.7 - 4, base[0] + nx * w * 1.5, base[1] + ny * w * 1.5);
    X.lineTo(base[0] - nx * w * 1.5, base[1] - ny * w * 1.5);
    X.quadraticCurveTo(base[0] - nx * w * 0.7, base[1] - ny * w * 0.7 - 4, top[0] - nx * w * 0.62, top[1] - ny * w * 0.62);
    X.closePath();
  };
  skFillShape(path, T, [top[0] + nx * w, top[1] + ny * w, top[0] - nx * w, top[1] - ny * w]);
  skClipIn(path, () => {
    // sternocleidomastoid groove
    X.save(); X.globalCompositeOperation = 'multiply';
    X.strokeStyle = 'rgba(120,55,45,0.30)'; X.lineWidth = w * 0.22; X.lineCap = 'round';
    X.beginPath(); X.moveTo(top[0] - nx * w * 0.25, top[1] - ny * w * 0.25 + 2);
    X.quadraticCurveTo((top[0] + base[0]) / 2 - nx * w * 0.1, (top[1] + base[1]) / 2 - ny * w * 0.1, base[0] - nx * w * 0.5, base[1] - ny * w * 0.5);
    X.stroke(); X.restore();
    fAO(top[0], top[1] + 2, w * 0.9, w * 0.5, 0.35);   // under-jaw shadow
  });
  if(opt.line !== false) skLine(path, T, 1.2, 0.3);
}

/* ============================================================
   HAIR: silky tapered ribbon tress (replaces uniform strokes)
   ============================================================ */
function tressS(x0, y0, c1x, c1y, c2x, c2y, x1, y1, w0, w1, col, sheen){
  // offset the centre bezier by a tapering width to build a ribbon
  const P = [[x0, y0], [c1x, c1y], [c2x, c2y], [x1, y1]];
  const off = (t, w) => {
    // approximate normal along cubic at t
    const mt = 1 - t;
    const bx = mt * mt * mt * P[0][0] + 3 * mt * mt * t * P[1][0] + 3 * mt * t * t * P[2][0] + t * t * t * P[3][0];
    const by = mt * mt * mt * P[0][1] + 3 * mt * mt * t * P[1][1] + 3 * mt * t * t * P[2][1] + t * t * t * P[3][1];
    const dx = 3 * mt * mt * (P[1][0] - P[0][0]) + 6 * mt * t * (P[2][0] - P[1][0]) + 3 * t * t * (P[3][0] - P[2][0]);
    const dy = 3 * mt * mt * (P[1][1] - P[0][1]) + 6 * mt * t * (P[2][1] - P[1][1]) + 3 * t * t * (P[3][1] - P[2][1]);
    const L = Math.hypot(dx, dy) || 1;
    return [bx - dy / L * w, by + dx / L * w];
  };
  const w = t => lerp(w0, w1, t);
  X.beginPath();
  let p = off(0, w(0)); X.moveTo(p[0], p[1]);
  for(let t = 0.2; t <= 1.001; t += 0.2){ p = off(t, w(t)); X.lineTo(p[0], p[1]); }
  for(let t = 1.0; t >= -0.001; t -= 0.2){ p = off(t, -w(t)); X.lineTo(p[0], p[1]); }
  X.closePath();
  X.fillStyle = col; X.fill();
  if(sheen){
    X.save(); X.globalCompositeOperation = 'soft-light';
    X.strokeStyle = sheen; X.lineWidth = Math.max(1, w0 * 0.3); X.lineCap = 'round';
    X.beginPath(); X.moveTo(x0, y0); X.bezierCurveTo(c1x, c1y, c2x, c2y, x1, y1); X.stroke(); X.restore();
  }
}
// Rounded scalp / hair mass with soft top light.
function hairMassS(cx, cy, rx, ry, rot, col){
  X.beginPath(); X.ellipse(cx, cy, rx, ry, rot, 0, TAU);
  const g = X.createRadialGradient(cx - rx * 0.3, cy - ry * 0.5, rx * 0.1, cx, cy, rx * 1.2);
  g.addColorStop(0, skLight(col, 0.22)); g.addColorStop(0.55, col); g.addColorStop(1, skDark(col, 0.4));
  X.fillStyle = g; X.fill();
}
