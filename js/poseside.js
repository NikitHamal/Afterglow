// Afterglow — module: poseside (loaded by index.html after poses.js)
// Side-view drawing for positions 1-6. Each figure is assembled from one
// continuous torso silhouette (spine curve + per-side widths), tapered limbs
// that root inside it, the shared portrait heads, and teardrop breasts — so the
// alternate positions read as the same people as the missionary scene instead
// of a stack of capsules.
'use strict';

/* ============================================================
   SHARED FIGURE PRIMITIVES
   ============================================================ */
// smooth Catmull-Rom run through pts (moveTo on first point when `move`)
function _cr(pts, move){
  const n = pts.length;
  if(move) X.moveTo(pts[0][0], pts[0][1]); else X.lineTo(pts[0][0], pts[0][1]);
  for(let i = 0; i < n - 1; i++){
    const p0 = pts[Math.max(i - 1, 0)], p1 = pts[i], p2 = pts[i + 1], p3 = pts[Math.min(i + 2, n - 1)];
    X.bezierCurveTo(p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6,
                    p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6, p2[0], p2[1]);
  }
}
// spine: [[x,y]...] head-end -> tail-end; wa/wb: half-widths on the side to the
// left of travel (A) and to the right (B). Returns the side normals for callers.
function bodyS(spine, wa, wb, T, opt){
  opt = opt || {};
  const k = opt.k || 1.7;                // the alternate scenes were drawn rail-thin
  wa = wa.map(v => v * k); wb = wb.map(v => v * k);
  const n = spine.length, nA = [], A = [], B = [];
  for(let i = 0; i < n; i++){
    const p = spine[Math.max(i - 1, 0)], q = spine[Math.min(i + 1, n - 1)];
    let tx = q[0] - p[0], ty = q[1] - p[1]; const L = Math.hypot(tx, ty) || 1; tx /= L; ty /= L;
    nA.push([ty, -tx]);
    A.push([spine[i][0] + ty * wa[i], spine[i][1] - tx * wa[i]]);
    B.push([spine[i][0] - ty * wb[i], spine[i][1] + tx * wb[i]]);
  }
  const t0 = [spine[1][0] - spine[0][0], spine[1][1] - spine[0][1]], l0 = Math.hypot(t0[0], t0[1]) || 1;
  const tn = [spine[n - 1][0] - spine[n - 2][0], spine[n - 1][1] - spine[n - 2][1]], ln = Math.hypot(tn[0], tn[1]) || 1;
  const capE = (wa[n - 1] + wb[n - 1]) * (opt.capEnd == null ? 0.7 : opt.capEnd);
  const capS = (wa[0] + wb[0]) * (opt.capStart == null ? 0.35 : opt.capStart);
  const path = () => {
    _cr(A, true);
    X.quadraticCurveTo(spine[n - 1][0] + tn[0] / ln * capE, spine[n - 1][1] + tn[1] / ln * capE, B[n - 1][0], B[n - 1][1]);
    _cr(B.slice().reverse(), false);
    X.quadraticCurveTo(spine[0][0] - t0[0] / l0 * capS, spine[0][1] - t0[1] / l0 * capS, A[0][0], A[0][1]);
    X.closePath();
  };
  // light axis across the middle of the form
  const m = n >> 1, wm = (wa[m] + wb[m]) / 2;
  const ax = skAxis(spine[m][0], spine[m][1], wm * 1.1, nA[m]);
  skForm(path, T, [ax[0], ax[1]], [ax[2], ax[3]], { edge: 0.26, rim: 0.18, cool: 0.12 });
  const litA = nA[m][0] * SK_LIT[0] + nA[m][1] * SK_LIT[1] > 0 ? 1 : -1;
  skClipIn(path, () => {
    for(let i = 0; i < n; i++){
      const [nx, ny] = nA[i], w = litA > 0 ? wa[i] : wb[i], wo = litA > 0 ? wb[i] : wa[i];
      const p = spine[Math.max(i - 1, 0)], q = spine[Math.min(i + 1, n - 1)];
      const rot = Math.atan2(q[1] - p[1], q[0] - p[0]);
      fHi(spine[i][0] + nx * litA * w * 0.42, spine[i][1] + ny * litA * w * 0.42, Math.max(w, 18) * 1.1, w * 0.34, 'rgba(255,240,226,0.15)', rot);
      fSh(spine[i][0] - nx * litA * wo * 0.72, spine[i][1] - ny * litA * wo * 0.72, Math.max(wo, 18) * 1.2, wo * 0.42, 'rgba(84,34,28,0.20)', rot);
    }
    if(opt.inner) opt.inner(spine, nA);
  });
  skLine(path, T, 1.2, 0.16);
  return { nA, A, B };
}
// two-segment limb with a joint crease, rooted at a (inside the torso)
function limb2(a, k, e, r0, r1, r2, T, opt){
  opt = opt || {};
  r0 *= 1.42; r1 *= 1.3; r2 *= 1.15;
  limbS(a, k, r0, r1, T, { belly: opt.belly || 1.1, crease: true, aoA: opt.aoA });
  limbS(k, e, r1 * 0.98, r2, T, { belly: opt.belly2 || 1.08 });
  // knee/elbow cap so the two segments read as one joint
  X.save(); X.globalCompositeOperation = 'screen';
  shade(k[0] - 2, k[1] - 3, r1 * 0.7, r1 * 0.5, 'rgba(255,236,220,0.12)', 0);
  X.restore();
}
// her face, placed and sized for the alternate positions
function herHeadAt(E, x, y, rot, s, neckTo){
  const T = herT();
  if(neckTo) neckS([x + 6 * s, y + 18 * s], neckTo, 11 * s, T);
  drawHerHead(E, { x, y, rot, s });
}
function hisHeadAt(x, y, rot, s, neckTo, mirror){
  const T = himT();
  if(neckTo) neckS([x + 2, y + 14 * s], neckTo, 14 * s, T);
  if(mirror){ X.save(); X.translate(x, y); X.scale(-1, 1); himHeadS(0, 0, rot, s); X.restore(); }
  else himHeadS(x, y, rot, s);
}
// long hair spilling from her head across a surface
function hairSpill(x0, y0, x1, y1, w){
  const hc = G.char ? G.char.hairColor : '#231318';
  tressS(x0, y0, lerp(x0, x1, 0.3), y0 + 10, lerp(x0, x1, 0.7), y1 - 8, x1, y1, w, 2, hc, 'rgba(255,210,190,0.14)');
  tressS(x0 + 6, y0 + 4, lerp(x0, x1, 0.4), y0 + 18, lerp(x0, x1, 0.8), y1, x1 + 10, y1 + 6, w * 0.7, 1.5, skDark(hc, 0.2));
}
function _thr(D, ax, ay, amp){ return [ax * D * amp, ay * D * amp]; }

/* ============================================================
   DISPATCH
   ============================================================ */
function drawPoseSide(){
  const E = herExpression(), br = Math.sin(G.t * TAU * (0.16 + G.pleasure * .004)) * 2.2;
  const p = G.pos | 0;
  if(p === 1) drawLegsUpSide(E, br);
  else if(p === 2) drawDoggySide(E, br);
  else if(p === 3) drawProneBoneSide(E, br);
  else if(p === 4) drawCowgirlSide(E, br, false);
  else if(p === 5) drawCowgirlSide(E, br, true);
  else drawSpoonSide(E, br);
}

/* POS 1 — LEGS-UP: on her back, hips lifted, legs over his shoulders */
function drawLegsUpSide(E, br){
  const D = G.depth || 0, H = herT(), HF = herFarT(), M = himT(), MF = himFarT();
  const jig = (G.breast.p || 0) * 0.8, imp = (G.impact || 0) * 3;
  const [tx, ty] = _thr(D, -1, 0.2, 26);
  const V = [636 - imp, 486];
  // her far leg, behind him
  limb2([622, 484], [700 + tx * 0.5, 372], [770 + tx * 0.6, 318], 20, 14, 8, HF);
  footS(776 + tx * 0.6, 312, -0.9, 0.9, HF);
  // him: kneeling, leaning into her
  const hp0 = [770 + tx, 512 + ty], sh = [716 + tx, 396 + ty];
  limb2([hp0[0] + 4, hp0[1] + 6], [760 + tx * 0.5, 560], [880, 568], 24, 16, 9, MF);
  footS(886, 568, 0.3, 0.95, MF);
  bodyS([sh, [736 + tx, 430 + ty], [752 + tx, 470 + ty], hp0], [16, 30, 26, 24], [16, 30, 22, 24], M);
  hisHeadAt(684 + tx, 360 + ty, -0.55, 1.15, [sh[0] - 6, sh[1] + 4]);
  // her torso on the sheets
  const sp_ = [[364, 524], [432, 516 + br * 0.3], [505, 519], [576, 507], [V[0] + 4, V[1] + 2]];
  bodyS(sp_, [10, 22, 17, 19, 14], [10, 22, 18, 24, 16], H);
  hairSpill(300, 500, 230, 548, 9);
  breastS(452, 486 + jig * 0.5, 17, Math.PI / 2 - 0.45, HF, { jig: 0 });
  breastS(470, 484 + jig * 0.6, 19, Math.PI / 2 - 0.3, H, { jig: 0 });
  poseVulva(V[0], V[1], 4 + 9 * D, 0.5);
  // shaft from his pelvis
  const B = [hp0[0] - 16, hp0[1] - 6], pu = 1 + .28 * (G.shaftPulse || 0);
  poseShaft(B, [lerp(B[0], V[0], .3 + .68 * D), lerp(B[1], V[1], .3 + .68 * D)], pu);
  // his near thigh + arm hooked round her lifted thigh
  limb2([hp0[0] + 2, hp0[1] + 4], [742 + tx * 0.5, 562], [866, 572], 26, 17, 10, M);
  footS(872, 572, 0.3, 1.0, M);
  // her near leg over his shoulder
  limb2([614, 490], [706 + tx * 0.6, 380 + ty * 0.5], [800 + tx * 0.7, 336], 22, 15, 8.5, H);
  footS(806 + tx * 0.7, 330, -0.75, 0.95, H);
  limb2(sh, [700 + tx, 446 + ty], [674 + tx * 0.6, 420], 14, 11, 7.5, M);
  handS(668 + tx * 0.6, 418, 4.6, 1.05, M, { curl: 0.7, spread: 0.3 });
  herHeadAt(E, 312, 498, -0.1, 0.98, [366, 524]);
  // her arms thrown up beside her head
  limb2([392, 516], [360, 470], [312, 452], 11, 9, 6, H);
  handS(306, 450, 4.0, 0.85, H, { curl: 0.5 });
}

/* POS 2 — DOGGY: on hands and knees, back arched, he kneels behind */
function drawDoggySide(E, br){
  const D = G.depth || 0, H = herT(), HF = herFarT(), M = himT(), MF = himFarT();
  const jig = (G.breast.p || 0) * 0.9, bj = (G.butt.p || 0) * 0.6;
  const [tx] = _thr(D, -1, 0, 30);
  const V = [646, 438 + bj * 0.3];
  // far limbs (behind her body)
  limb2([402, 466], [400, 512], [396, 552], 12, 9.5, 6, HF);
  handS(388, 556, 3.3, 0.9, HF, { curl: 0.35 });
  limb2([606, 448], [598, 556], [700, 566], 22, 15, 8, HF);
  footS(708, 564, 0.15, 0.9, HF);
  // her torso: neck -> chest -> arched waist -> raised hips
  const sp_ = [[372, 462], [438, 458 + br * 0.2], [515, 448], [586, 428 + bj * 0.4], [V[0] - 2, V[1] - 4]];
  bodyS(sp_, [9, 17, 13, 24, 16], [10, 24, 17, 18, 12], H);
  poseGlute(612, 420 + bj, 38, 36, 0, -0.15);
  poseVulva(V[0], V[1], 4 + 9 * D, 0.2);
  // hanging breasts swaying under her
  breastS(436, 490 + jig * 0.5, 17, -Math.PI / 2 - 0.12, HF);
  breastS(452, 494 + jig * 0.6, 19, -Math.PI / 2 - 0.08, H);
  // near thigh + shin folded on the bed
  limb2([600, 452], [588, 558], [690, 570], 24, 16, 9, H);
  footS(698, 568, 0.12, 0.95, H);
  // near arm braced
  limb2([400, 470], [394, 514], [388, 556], 13, 10, 6.5, H);
  handS(380, 560, 3.25, 0.95, H, { curl: 0.3 });
  herHeadAt(E, 330, 436, -0.15, 0.98, [376, 462]);
  hairSpill(312, 446, 336, 520, 7);
  // him: kneeling upright behind her
  const hp0 = [736 + tx, 474], sh = [732 + tx * 0.6, 348];
  limb2([hp0[0] + 8, hp0[1] + 6], [770 + tx * 0.5, 562], [872, 570], 24, 16, 9, MF);
  footS(880, 570, 0.2, 0.95, MF);
  bodyS([sh, [728 + tx * 0.7, 392], [732 + tx * 0.85, 436], hp0], [17, 30, 24, 24], [17, 27, 22, 26], M);
  hisHeadAt(716 + tx * 0.6, 302, -0.18, 1.15, [sh[0] - 2, sh[1] + 2]);
  const B = [hp0[0] - 20, hp0[1] - 6], pu = 1 + .28 * (G.shaftPulse || 0);
  poseShaft(B, [lerp(B[0], V[0], .3 + .68 * D), lerp(B[1], V[1], .3 + .68 * D)], pu);
  limb2([hp0[0] + 4, hp0[1] + 6], [754 + tx * 0.5, 564], [860, 574], 26, 17, 10, M);
  footS(868, 574, 0.2, 1.0, M);
  // hands on her hips
  limb2([sh[0] - 6, sh[1] + 12], [716 + tx * 0.7, 420], [650 + tx * 0.3, 418 + bj], 14, 11, 7.5, M);
  handS(642 + tx * 0.3, 418 + bj, 4.55, 1.05, M, { curl: 0.7, spread: 0.3 });
}

/* POS 3 — PRONE: flat on her stomach, he lies over her */
function drawProneBoneSide(E, br){
  const D = G.depth || 0, H = herT(), HF = herFarT(), M = himT(), MF = himFarT();
  const bj = (G.butt.p || 0) * 0.6;
  const [tx] = _thr(D, -1, 0, 24);
  const V = [652, 516];
  // her body flat on the sheets, cheek on the pillow
  limb2([636, 530], [778, 546], [896, 552], 20, 14, 8, HF);
  footS(904, 548, -0.4, 0.85, HF);
  const sp_ = [[360, 530], [432, 534], [514, 536], [590, 526], [V[0], V[1] + 10]];
  bodyS(sp_, [9, 16, 12, 22, 14], [10, 20, 16, 14, 12], H);
  poseGlute(606, 512 + bj, 40, 26, 0, 0.05);
  breastS(446, 556, 15, -Math.PI / 2 + 0.6, H);
  limb2([640, 532], [784, 544], [906, 554], 22, 15, 8.5, H);
  footS(914, 550, -0.4, 0.9, H);
  poseVulva(V[0], V[1], 3 + 8 * D, 0.25);
  herHeadAt(E, 306, 508, -0.18, 0.98, [362, 530]);
  hairSpill(290, 486, 220, 540, 9);
  // contact shadow where his chest presses on her back
  X.save(); X.globalCompositeOperation = 'multiply';
  shade(510 + tx, 514, 150, 12, 'rgba(64,30,24,.34)', -.02);
  X.restore();
  // him: prone over her, up on his forearms
  const hp0 = [720 + tx, 498], sh = [446 + tx, 478];
  limb2([hp0[0] + 4, hp0[1] + 4], [852 + tx * 0.5, 520], [962, 540], 22, 15, 8.5, MF);
  footS(970, 538, -0.3, 0.9, MF);
  bodyS([sh, [520 + tx, 482], [600 + tx, 488], [670 + tx, 492], hp0], [15, 26, 22, 20, 18], [15, 24, 20, 20, 18], M);
  const B = [hp0[0] - 18, hp0[1] + 6], pu = 1 + .28 * (G.shaftPulse || 0);
  poseShaft(B, [lerp(B[0], V[0], .3 + .68 * D), lerp(B[1], V[1], .3 + .68 * D)], pu);
  limb2([hp0[0] + 2, hp0[1] + 6], [840 + tx * 0.5, 528], [950, 548], 24, 16, 9, M);
  footS(958, 546, -0.3, 0.95, M);
  hisHeadAt(406 + tx, 448, -0.45, 1.12, [sh[0] - 4, sh[1]]);
  limb2([sh[0] + 6, sh[1] + 10], [470 + tx * 0.6, 536], [400, 548], 14, 11, 7.5, M);
  handS(392, 548, 4.5, 1.0, M, { curl: 0.45, spread: 0.4 });
}

/* POS 4/5 — COWGIRL / REVERSE: she straddles him. rev = facing his feet */
function drawCowgirlSide(E, br, rev){
  const D = G.depth || 0, H = herT(), HF = herFarT(), M = himT(), MF = himFarT();
  const jig = (G.breast.p || 0) * 0.95, bj = (G.butt.p || 0) * 0.5;
  const hy = 506 - 40 * D;                     // her hips ride up and down
  const lean = rev ? 0.20 : 0.10;              // forward lean (toward +x)
  // him lying on his back; head to the right (cowgirl) or left (reverse)
  const dir = rev ? -1 : 1;
  const hx = 706;
  const mSpine = rev
    ? [[500, 534], [572, 530], [640, 532], [hx + 8, 538]]
    : [[912, 534], [840, 530], [772, 532], [hx - 8, 538]];
  limb2([hx, 548], [hx - dir * 120, 540], [hx - dir * 240, 556], 22, 15, 9, MF);
  footS(hx - dir * 248, 552, dir > 0 ? Math.PI + 0.4 : -0.4, 0.95, MF);
  bodyS(mSpine, [17, 27, 24, 22], [15, 22, 20, 20], M);
  hisHeadAt(rev ? 452 : 960, 522, Math.PI / 2 - 0.15, 1.15, null, rev);
  limb2([hx, 552], [hx - dir * 116, 548], [hx - dir * 236, 562], 24, 16, 10, M);
  footS(hx - dir * 244, 558, dir > 0 ? Math.PI + 0.4 : -0.4, 1.0, M);
  const pu = 1 + .28 * (G.shaftPulse || 0);
  poseShaft([hx + 2, 534], [hx + 2, hy + 10], pu);
  // her far leg folded beside him
  limb2([hx + 4, hy + 4], [hx + 70, 552], [hx - 34, 562], 20, 14, 8, HF);
  footS(hx - 40, 560, Math.PI + 0.1, 0.85, HF);
  // her torso upright, leaning toward +x
  const top = [hx + 4 + Math.sin(lean) * 170, hy - 166];
  const sp_ = [top, [lerp(top[0], hx, 0.3), hy - 118], [lerp(top[0], hx, 0.62), hy - 66], [hx + 2, hy - 18], [hx + 2, hy + 8]];
  // front faces +x: travel is downward, so side A (left of travel) is +x
  bodyS(sp_, [9, 22, 15, 17, 12], [9, 20, 14, 24, 16], H);
  poseGlute(hx - 14, hy - 2 + bj, 30, 30, 0, 0.1);
  breastS(sp_[1][0] + 22, sp_[1][1] + 2 + jig * 0.6, 17, Math.PI + 0.25, HF);
  breastS(sp_[1][0] + 26, sp_[1][1] + 8 + jig * 0.7, 19, Math.PI + 0.35, H);
  // her near leg: knee forward on the bed, shin tucked back
  limb2([hx + 2, hy + 6], [hx + 84, 548], [hx - 22, 566], 22, 15, 8.5, H);
  footS(hx - 30, 564, Math.PI + 0.1, 0.9, H);
  // arms: braced forward on him (rev: on his knees)
  const sh = [top[0] - 2, top[1] + 16];
  const hand = rev ? [hx + 118, 522] : [hx + 92, 520];
  limb2(sh, [lerp(sh[0], hand[0], 0.5) + 10, lerp(sh[1], hand[1], 0.5) - 6], hand, 11, 9, 6, H);
  handS(hand[0] + 4, hand[1] + 4, 2.0, 0.9, H, { curl: 0.45 });
  herHeadAt(E, top[0] + (rev ? 6 : 10), top[1] - 40, rev ? 0.12 : -0.05, 0.98, [top[0], top[1] + 4]);
  hairSpill(top[0] - 18, top[1] - 50, top[0] - 26, top[1] + 30, 8);
}

/* POS 6 — SPOONING: both on their sides, he holds her from behind */
function drawSpoonSide(E, br){
  const D = G.depth || 0, H = herT(), HF = herFarT(), M = himT(), MF = himFarT();
  const [tx] = _thr(D, -1, 0, 18);
  const V = [640, 526];
  // him behind (further up the bed in screen space)
  const hp0 = [690 + tx, 506], sh = [440 + tx * 0.3, 492];
  limb2([hp0[0], hp0[1] + 4], [778 + tx * 0.5, 530], [872, 540], 22, 15, 9, MF);
  footS(880, 538, 0.1, 0.9, MF);
  bodyS([sh, [516 + tx * 0.5, 496], [600 + tx * 0.8, 500], hp0], [14, 26, 22, 24], [14, 24, 20, 20], M);
  hisHeadAt(402 + tx * 0.3, 466, -0.12, 1.12, [sh[0] - 4, sh[1]]);
  const B = [hp0[0] - 30, hp0[1] + 10], pu = 1 + .28 * (G.shaftPulse || 0);
  poseShaft(B, [lerp(B[0], V[0], .3 + .68 * D), lerp(B[1], V[1], .3 + .68 * D)], pu);
  // her: on her side, hip rising in a long curve
  const sp_ = [[356, 524], [430, 526], [508, 532], [580, 520], [V[0] + 4, V[1] + 4]];
  bodyS(sp_, [9, 18, 13, 26, 16], [10, 22, 16, 18, 14], H);
  poseGlute(612, 516, 32, 30, 0, 0);
  breastS(424, 548, 17, -0.6, H);
  limb2([626, 526], [712, 552], [800, 556], 22, 15, 8.5, H);
  footS(808, 554, 0.1, 0.9, H);
  poseVulva(V[0], V[1], 3 + 8 * D, 0.2);
  herHeadAt(E, 304, 504, -0.08, 0.98, [358, 524]);
  // his arm wrapped over her waist
  X.save(); X.globalCompositeOperation = 'multiply';
  shade(560, 512, 60, 9, 'rgba(64,30,24,.30)', .02);
  X.restore();
  limb2([520 + tx * 0.5, 498], [560 + tx * 0.3, 512], [604, 528], 14, 11, 7.5, M);
  handS(610, 530, 1.75, 0.95, M, { curl: 0.6, spread: 0.3 });
}
