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
// The ramp is deliberately wide (lit cheek ~2x the core shadow) because that
// value range is what makes a silhouette read as a lit three-dimensional volume
// instead of flat pigment.
function skTone(base){
  const d = skDark(base, 0.58);
  return { hi: skLight(base, 0.50), b: base, s: skDark(base, 0.30), d, dk: d };
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
  const g = gLinear(lg[0], lg[1], lg[2], lg[3],
    [[0, T.hi], [0.38, T.b], [0.78, T.s], [1, T.d]]);
  X.fillStyle = g; X.fill();
}
// Fill a caller-defined closed path with a radial skin gradient (heads, domes).
function skFillRad(fn, T, cx, cy, r0, r1){
  X.beginPath(); fn();
  const g = gRadial(cx, cy, r0, cx, cy, r1,
    [[0, T.hi], [0.45, T.b], [0.82, T.s], [1, T.d]]);
  X.fillStyle = g; X.fill();
}
// Run shading callbacks clipped inside a caller-defined path.
function skClipIn(fn, inner){ X.save(); X.beginPath(); fn(); X.clip(); inner(); X.restore(); }
// Soft multiply form-shadow blob.
function fSh(x, y, rx, ry, col, rot){ X.save(); X.globalCompositeOperation = 'multiply'; shade(x, y, rx, ry, col, rot || 0); X.restore(); }
// Soft screen highlight blob.
function fHi(x, y, rx, ry, col, rot){ X.save(); X.globalCompositeOperation = 'screen'; shade(x, y, rx, ry, col, rot || 0); X.restore(); }
// Warm subsurface glow at shadow terminator.
function fSSS(x, y, rx, ry, rot){ X.save(); X.globalCompositeOperation = 'soft-light'; shade(x, y, rx, ry, 'rgba(255,135,105,0.32)', rot || 0); X.restore(); }
// Contact / ambient-occlusion pool.
function fAO(x, y, rx, ry, a, rot){ fSh(x, y, rx, ry, `rgba(30,10,12,${a})`, rot); }
// Delicate silhouette line so forms read against dark bg (hentai linework).
function skLine(fn, T, w, a){ X.save(); X.beginPath(); fn(); X.strokeStyle = `rgba(${hexToRgb(T.dk).join(',')},${a == null ? 0.4 : a})`; X.lineWidth = w || 1.4; X.stroke(); X.restore(); }

/* ============================================================
   FORM PASS — the volume treatment every body shape runs through
   ------------------------------------------------------------
   A shape filled with a single gradient still looks like a cut-out: real lit
   forms lose brightness again at BOTH silhouette edges (limb darkening) and
   catch a rim where the key and the moon graze the outline. L/S are the two
   ends of the light axis in the CURRENT path coordinates; everything else is
   proportional to that axis, so the same call works at any size.
   ============================================================ */
function skContour(fn, L, S, opt){
  opt = opt || {};
  const e = opt.edge == null ? 0.30 : opt.edge;
  if(e > 0){
    X.save(); X.globalCompositeOperation = 'multiply';
    X.beginPath(); fn();
    X.fillStyle = gLinear(L[0], L[1], S[0], S[1], [
      [0, `rgba(74,30,26,${e * 0.80})`], [0.11, 'rgba(74,30,26,0)'],
      [0.76, 'rgba(74,30,26,0)'], [1, `rgba(74,30,26,${e})`]]);
    X.fill(); X.restore();
  }
  const rm = opt.rim == null ? 0.20 : opt.rim;
  if(rm > 0){
    X.save(); X.globalCompositeOperation = 'screen';
    X.beginPath(); fn();
    X.fillStyle = gLinear(L[0], L[1], S[0], S[1], [
      [0, 'rgba(255,238,220,0)'], [0.085, `rgba(255,241,224,${rm})`],
      [0.24, 'rgba(255,238,220,0)'], [1, 'rgba(0,0,0,0)']]);
    X.fill(); X.restore();
  }
  const cs = opt.cool == null ? 0.15 : opt.cool;
  if(cs > 0){
    X.save(); X.globalCompositeOperation = 'screen';
    X.beginPath(); fn();
    X.fillStyle = gLinear(L[0], L[1], S[0], S[1], [
      [0, 'rgba(0,0,0,0)'], [0.86, 'rgba(0,0,0,0)'],
      [0.965, `rgba(188,214,255,${cs})`], [1, 'rgba(188,214,255,0)']]);
    X.fill(); X.restore();
  }
}
// Directional body fill + the volume pass, in one call.
function skForm(fn, T, L, S, opt){
  skFillShape(fn, T, [L[0], L[1], S[0], S[1]]);
  skContour(fn, L, S, opt);
}
// Light axis across a form: returns [Lx,Ly,Sx,Sy] for a shape centred at
// (cx,cy) whose cross-section radius is r. `axis` picks the cross-section
// direction — 'auto' uses the light vector itself (works for any orientation).
function skAxis(cx, cy, r, axis, lit){
  lit = lit || SK_LIT;
  let ax = lit[0], ay = lit[1];
  if(axis){
    // project the light onto the given cross-section normal
    const d = lit[0] * axis[0] + lit[1] * axis[1];
    const s = d < 0 ? -1 : 1;
    ax = axis[0] * s; ay = axis[1] * s;
  }
  return [cx + ax * r, cy + ay * r, cx - ax * r, cy - ay * r];
}

/* ============================================================
   FEATHERED LINEWORK
   ------------------------------------------------------------
   A canvas stroke at a constant alpha with round caps reads as a plastic rod
   lying on the skin. Anatomical lines — ribs, creases, tendons, the areolar
   border — have to taper and fade at both ends, so they go down as a chain of
   soft gradient blobs along the curve instead.
   rgb = 'r,g,b' string · a = peak alpha · opt{n, up, rot, wide}
   ============================================================ */
function skArc(x0, y0, qx, qy, x1, y1, w, rgb, a, opt){
  opt = opt || {};
  // Blob spacing must stay well under the blob radius or the line breaks up
  // into polka dots, so the count follows the arc's actual length.
  const len = Math.hypot(qx - x0, qy - y0) + Math.hypot(x1 - qx, y1 - qy);
  const n = opt.n || clamp(Math.ceil(len / Math.max(3, w * 0.55)), 8, 20);
  // ...and the blob width must exceed the blob spacing, whatever n the caller
  // picked. Scaling `wide` to the spacing keeps any arc continuous instead of
  // dotted, which is what lets long anatomical lines stay cheap (low n).
  const spacing = len / n;
  const wide = Math.max(opt.wide == null ? 0.85 : opt.wide, 1.55 * spacing / Math.max(1, w));
  X.save();
  X.globalCompositeOperation = opt.up ? 'screen' : 'multiply';
  for(let i = 0; i <= n; i++){
    const t = i / n, mt = 1 - t;
    const x = mt * mt * x0 + 2 * mt * t * qx + t * t * x1;
    const y = mt * mt * y0 + 2 * mt * t * qy + t * t * y1;
    const e = Math.sin(Math.PI * (0.24 + 0.62 * t));   // taper both ends
    shade(x, y, w * wide * (0.5 + 0.6 * e), w * (opt.thin || 0.60),
      `rgba(${rgb},${(a * e).toFixed(3)})`, opt.rot || 0);
  }
  X.restore();
}

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
  const L = [cx + lit[0] * R, cy + lit[1] * R], S = [cx - lit[0] * R, cy - lit[1] * R];
  skForm(path, T, L, S, {
    edge: opt.edge == null ? 0.30 : opt.edge,
    rim: opt.rim == null ? 0.20 : opt.rim,
    cool: opt.cool == null ? 0.14 : opt.cool
  });

  skClipIn(path, () => {
    // specular crest toward light — softer, broader
    fHi(cx + lit[0] * rm * 0.48, cy + lit[1] * rm * 0.48, len * 0.44, rm * 0.52, 'rgba(255,242,228,0.22)', Math.atan2(uy, ux));
    fHi(cx + lit[0] * rm * 0.52, cy + lit[1] * rm * 0.52, len * 0.28, rm * 0.28, 'rgba(255,252,245,0.18)', Math.atan2(uy, ux));
    // warm SSS terminator — subtle peach bleed
    fSSS(cx - lit[0] * rm * 0.30, cy - lit[1] * rm * 0.30, len * 0.38, rm * 0.55, Math.atan2(uy, ux));
    // core shadow on far edge — less burnt, more depth
    fSh(cx - lit[0] * rm * 0.85, cy - lit[1] * rm * 0.85, len * 0.52, rm * 0.60, 'rgba(70,26,22,0.28)', Math.atan2(uy, ux));
    fSh(cx - lit[0] * rm * 0.65, cy - lit[1] * rm * 0.65, len * 0.42, rm * 0.42, 'rgba(90,36,28,0.14)', Math.atan2(uy, ux));
    // knee / elbow crease hint: a soft transverse dip just inside the far end
    if(opt.crease) fSh(b[0] - ux * r2 * 0.9, b[1] - uy * r2 * 0.9, r2 * 1.6, r2 * 0.5, 'rgba(96,42,32,0.20)', Math.atan2(uy, ux));
    if(opt.aoA) fAO(a[0], a[1], r1 * 1.25, r1 * 1.25, opt.aoA);
    if(opt.aoB) fAO(b[0], b[1], r2 * 1.25, r2 * 1.25, opt.aoB);
  });
  if(opt.line !== false) skLine(path, T, 1.2, 0.22);
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
  skClipIn(palm, () => {
    fHi(-2, -3, 7, 5, 'rgba(255,240,225,0.22)');
    fHi(-1, -2, 4, 2.2, 'rgba(255,252,245,0.20)');
    fAO(0, 8, 9, 4, 0.22);
    // thenar & hypothenar subtle volume
    fSh(-5, 4, 4, 3, 'rgba(120,65,55,0.12)', -0.6);
    fSh(5, 4, 4, 3, 'rgba(120,65,55,0.10)', 0.6);
  });
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
    X.fillStyle = 'rgba(255,240,232,0.65)';           // nail with lunula
    X.beginPath(); X.ellipse(j2[0], j2[1] + 0.4, 1.35, 1.05, fa, 0, TAU); X.fill();
    X.fillStyle = 'rgba(255,255,255,0.42)';
    X.beginPath(); X.ellipse(j2[0] - 0.2, j2[1] - 0.1, 0.75, 0.55, fa, 0, TAU); X.fill();
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
  // radial form light from upper-outer — softer
  X.beginPath(); path();
  const g = X.createRadialGradient(-r * 0.38, -r * 0.42, r * 0.08, 0, 0, r * 1.48);
  g.addColorStop(0, T.hi); g.addColorStop(0.38, T.b); g.addColorStop(0.74, T.s); g.addColorStop(1, T.d);
  X.fillStyle = g; X.fill();
  // rim/limb-darkening so the dome has a real edge instead of a sticker outline
  skContour(path, [-r * 1.15 * 0.62, -r * 1.15 * 0.78], [r * 1.15 * 0.62, r * 1.15 * 0.78],
    { edge: 0.18, rim: 0.16, cool: 0.16 });
  skClipIn(path, () => {
    fHi(-r * 0.38, -r * 0.44, r * 0.48, r * 0.32, 'rgba(255,244,232,0.28)', -0.5);   // crest specular
    fHi(-r * 0.30, -r * 0.36, r * 0.28, r * 0.18, 'rgba(255,255,250,0.18)', -0.5);
    fSh(r * 0.08, r * 0.82, r * 0.90, r * 0.38, 'rgba(90,35,30,0.32)', 0.12);          // under-breast shadow
    fSSS(-r * 0.88, r * 0.32, r * 0.48, r * 0.38, 0.6);                                // rim SSS
    fAO(r * 0.82, 0, r * 0.38, r * 0.88, 0.24);                                       // chest-wall contact
  });
  // areola + nipple on the apex, squashed by viewing angle
  const aer = r * (0.34 + 0.06 * ar);
  X.save(); X.translate(-r * 0.92, r * 0.02); X.rotate(0.1);
  // areola with soft halo + Montgomery dots + nipple SSS
  const areolaBase = skDark(G.char ? G.char.nippleColor : '#c25f63', 0.08);
  const ag2 = X.createRadialGradient(0, 0, aer * 0.15, 0, 0, aer * 1.1);
  ag2.addColorStop(0, `rgba(${hexToRgb(G.char ? G.char.nippleColor : '#c25f63').join(',')},0.62)`);
  ag2.addColorStop(0.55, `rgba(${hexToRgb(areolaBase).join(',')},0.38)`);
  ag2.addColorStop(1, `rgba(${hexToRgb(areolaBase).join(',')},0)`);
  X.fillStyle = ag2;
  X.beginPath(); X.ellipse(0, 0, aer * 0.70, aer * 0.95, 0, 0, TAU); X.fill();
  X.fillStyle = G.char ? G.char.nippleColor : '#c25f63';
  X.beginPath(); X.ellipse(-aer * 0.12, 0, aer * 0.30, aer * 0.42 + ar * 1.6, 0, 0, TAU); X.fill();
  // Montgomery tubercles
  X.fillStyle = `rgba(${hexToRgb(skDark(G.char ? G.char.nippleColor : '#c25f63', 0.18)).join(',')},0.22)`;
  for(let k=0;k<6;k++){ const a=k/6*TAU, rr=aer*0.52; X.beginPath(); X.arc(Math.cos(a)*rr*0.62, Math.sin(a)*rr*0.92, 0.55, 0, TAU); X.fill(); }
  X.fillStyle = 'rgba(255,242,240,0.42)';
  X.beginPath(); X.arc(-aer * 0.28, -aer * 0.22, aer * 0.13, 0, TAU); X.fill();
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
    // side: single cleft with swollen labia edge — softer
    X.strokeStyle = `rgba(${185 + 22 * eng | 0},105,100,0.62)`; X.lineWidth = 2.8; X.lineCap = 'round';
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
// Rounded scalp / hair mass. Three offset lobes instead of one ellipse: a single
// ellipse gives hair a helmet silhouette with a machined edge, which is the
// loudest cartoon tell left on a portrait once the skin is properly shaded.
function hairMassS(cx, cy, rx, ry, rot, col){
  const lobes = [
    [-0.32, -0.12, 0.80, 0.90],
    [ 0.30, -0.16, 0.76, 0.84],
    [ 0.02,  0.16, 0.92, 0.96]
  ];
  X.save();
  X.translate(cx, cy); X.rotate(rot);
  for(let i = 0; i < lobes.length; i++){
    const L = lobes[i];
    X.beginPath();
    X.ellipse(L[0] * rx, L[1] * ry, rx * L[2], ry * L[3], (i - 1) * 0.15, 0, TAU);
    const g = X.createRadialGradient(-rx * 0.30, -ry * 0.52, rx * 0.08,
                                     L[0] * rx, L[1] * ry, rx * L[2] * 1.28);
    g.addColorStop(0, skLight(col, 0.18));
    g.addColorStop(0.52, col);
    g.addColorStop(1, skDark(col, 0.44));
    X.fillStyle = g; X.fill();
  }
  // lamp sheen on the upper-left of the crown
  X.globalCompositeOperation = 'soft-light';
  X.beginPath(); X.ellipse(-rx * 0.34, -ry * 0.40, rx * 0.44, ry * 0.30, -0.5, 0, TAU);
  X.fillStyle = 'rgba(255,206,216,0.28)'; X.fill();
  X.restore();
}
