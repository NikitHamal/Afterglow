// Afterglow — module: atmos (loaded by index.html after gfx.js)
// Bedroom set, bed, practical lights and the screen-space post pass.
// Everything static is painted once into device-resolution offscreen layers;
// per frame we only blit those and add the living parts (flicker, moonbeam,
// motes, curtain sway, bloom, grade, grain).
'use strict';

/* ============================================================
   OFFSCREEN LAYER CACHE
   ============================================================ */
const ATM = { w: 0, h: 0, room: null, bed: null, grain: null, small: null, small2: null,
  ghost: null, ghostA: 0, sig: '', flick: 1, lampT: 0 };

function _atmCanvas(w, h){
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  return c;
}
// Paint a static layer: the shared helpers (shade, gLinear, skArc…) all draw on
// the global X, so the layer is painted onto the live canvas at the start of a
// frame (before anything else) and copied out into its cache.
function _atmPaint(cv, fn){
  X.save();
  X.setTransform(CV.width / W, 0, 0, CV.height / H, 0, 0);
  X.clearRect(0, 0, W, H);
  fn(X);
  X.restore();
  const c = cv.getContext('2d');
  c.globalCompositeOperation = 'copy';
  c.drawImage(CV, 0, 0);
  X.save(); X.setTransform(1, 0, 0, 1, 0, 0); X.clearRect(0, 0, CV.width, CV.height); X.restore();
}
// Must run at the start of draw(), right after the canvas is cleared.
function atmEnsure(){
  if(ATM.w === CV.width && ATM.h === CV.height && ATM.room) return;
  ATM.w = CV.width; ATM.h = CV.height;
  ATM.room = _atmCanvas(ATM.w, ATM.h);
  _atmPaint(ATM.room, paintRoomStatic);
  // soft depth-of-field: the far wall sits a little out of focus
  const sharp = ATM.room;
  ATM.room = _atmCanvas(ATM.w, ATM.h);
  const rc = ATM.room.getContext('2d');
  rc.filter = `blur(${Math.max(0.6, ATM.w / W * 1.1)}px)`;
  rc.drawImage(sharp, 0, 0);
  rc.filter = 'none';
  ATM.bed = _atmCanvas(ATM.w, ATM.h);
  _atmPaint(ATM.bed, paintBedStatic);
  ATM.small = _atmCanvas(320, 180);
  ATM.small2 = _atmCanvas(320, 180);
  if(!ATM.ghost || ATM.ghost.width !== ATM.w) ATM.ghost = _atmCanvas(ATM.w, ATM.h);
}
function _blit(cv, a){
  X.save(); X.setTransform(1, 0, 0, 1, 0, 0);
  if(a != null) X.globalAlpha = a;
  X.drawImage(cv, 0, 0);
  X.restore();
}
// seeded hash so the static set is identical on every rebuild
function _h(i){ const s = Math.sin(i * 127.1 + 311.7) * 43758.5453; return s - Math.floor(s); }

/* ============================================================
   STATIC ROOM
   ============================================================ */
const WIN = { x: 1030, y: 60, w: 200, h: 260 };

function paintRoomStatic(ctx){
  // wall: deep aubergine with a warm lift toward the lamp side
  ctx.fillStyle = gLinear(0, 0, 0, 560, [[0, '#0e080b'], [0.55, '#170c12'], [1, '#1f1117']]);
  ctx.fillRect(0, 0, W, H);
  ctx.save(); ctx.globalCompositeOperation = 'screen';
  ctx.fillStyle = gRadial(140, 420, 10, 140, 420, 900, [[0, 'rgba(120,58,34,0.30)'], [0.5, 'rgba(70,30,26,0.10)'], [1, 'rgba(0,0,0,0)']]);
  ctx.fillRect(0, 0, W, H);
  ctx.restore();

  // damask wallpaper: faint repeating lozenge motif
  ctx.save(); ctx.globalAlpha = 0.05; ctx.strokeStyle = '#d9a68a'; ctx.lineWidth = 1;
  for(let y = 96; y < 380; y += 46){
    for(let x = ((y / 46) % 2) * 32; x < W; x += 64){
      ctx.beginPath();
      ctx.moveTo(x, y - 16); ctx.quadraticCurveTo(x + 10, y, x, y + 16);
      ctx.quadraticCurveTo(x - 10, y, x, y - 16);
      ctx.moveTo(x - 6, y); ctx.lineTo(x + 6, y);
      ctx.stroke();
    }
  }
  ctx.restore();

  // crown molding
  ctx.fillStyle = gLinear(0, 0, 0, 78, [[0, '#060305'], [0.8, '#120a0e'], [1, '#2a171d']]);
  ctx.fillRect(0, 0, W, 78);
  ctx.fillStyle = 'rgba(255,200,160,0.06)'; ctx.fillRect(0, 76, W, 1.5);
  ctx.fillStyle = 'rgba(0,0,0,0.5)'; ctx.fillRect(0, 78, W, 3);

  // wainscot: chair rail + recessed panels
  ctx.fillStyle = gLinear(0, 380, 0, 560, [[0, '#1a0e13'], [1, '#22131a']]);
  ctx.fillRect(0, 384, W, 180);
  ctx.fillStyle = 'rgba(255,190,150,0.07)'; ctx.fillRect(0, 380, W, 2);
  ctx.fillStyle = 'rgba(0,0,0,0.45)'; ctx.fillRect(0, 384, W, 3);
  for(let x = 26; x < W - 60; x += 176){
    ctx.strokeStyle = 'rgba(0,0,0,0.38)'; ctx.lineWidth = 2;
    ctx.strokeRect(x, 404, 150, 120);
    ctx.strokeStyle = 'rgba(255,190,150,0.05)'; ctx.lineWidth = 1;
    ctx.strokeRect(x + 2, 406, 150, 120);
  }

  // framed art: abstract warm nude-line study (just two curves of light)
  const fx = 330, fy = 128, fw = 150, fh = 108;
  ctx.fillStyle = 'rgba(0,0,0,0.45)'; ctx.fillRect(fx + 6, fy + 8, fw, fh);
  ctx.fillStyle = gLinear(fx, fy, fx + fw, fy + fh, [[0, '#6a4a2c'], [0.5, '#3a2416'], [1, '#5a3a20']]);
  ctx.fillRect(fx - 7, fy - 7, fw + 14, fh + 14);
  ctx.fillStyle = gLinear(fx, fy, fx, fy + fh, [[0, '#2a1a1c'], [1, '#1a0f12']]);
  ctx.fillRect(fx, fy, fw, fh);
  ctx.strokeStyle = 'rgba(232,170,140,0.45)'; ctx.lineWidth = 2.2; ctx.lineCap = 'round';
  ctx.beginPath(); ctx.moveTo(fx + 22, fy + 80); ctx.bezierCurveTo(fx + 50, fy + 20, fx + 90, fy + 96, fx + 128, fy + 34); ctx.stroke();
  ctx.strokeStyle = 'rgba(255,120,140,0.30)'; ctx.lineWidth = 1.4;
  ctx.beginPath(); ctx.moveTo(fx + 30, fy + 64); ctx.bezierCurveTo(fx + 66, fy + 40, fx + 82, fy + 76, fx + 120, fy + 58); ctx.stroke();
  ctx.fillStyle = 'rgba(255,200,160,0.10)'; ctx.fillRect(fx - 7, fy - 7, fw + 14, 2);

  paintWindowStatic(ctx);

  // nightstand: walnut with drawer and lamp-lit top edge
  ctx.fillStyle = gLinear(48, 465, 206, 585, [[0, '#2a161b'], [1, '#150a0e']]);
  ctx.fillRect(48, 465, 158, 120);
  ctx.fillStyle = gLinear(40, 458, 40, 470, [[0, '#4a2a22'], [1, '#2a161b']]);
  ctx.fillRect(40, 458, 174, 10);
  ctx.fillStyle = 'rgba(255,200,140,0.22)'; ctx.fillRect(40, 458, 174, 1.5);
  ctx.strokeStyle = 'rgba(0,0,0,0.5)'; ctx.lineWidth = 2; ctx.strokeRect(66, 488, 120, 34);
  ctx.strokeStyle = 'rgba(255,190,140,0.08)'; ctx.lineWidth = 1; ctx.strokeRect(67, 489, 120, 34);
  ctx.fillStyle = '#c9a56a'; ctx.beginPath(); ctx.arc(126, 505, 3.8, 0, TAU); ctx.fill();
  ctx.fillStyle = 'rgba(255,255,230,0.85)'; ctx.beginPath(); ctx.arc(125, 504, 1.2, 0, TAU); ctx.fill();

  // books + wine glass on the nightstand
  [['#4a1620', 0], ['#22303a', 1], ['#5a3a1c', 2]].forEach(([c, i]) => {
    ctx.fillStyle = c; ctx.fillRect(160 - i * 2, 448 - i * 7, 40 + i * 3, 7);
    ctx.fillStyle = 'rgba(255,220,180,0.16)'; ctx.fillRect(160 - i * 2, 448 - i * 7, 40 + i * 3, 1);
  });
  // glass: bowl, stem, foot, a little red left in it
  ctx.save();
  ctx.strokeStyle = 'rgba(255,230,210,0.35)'; ctx.lineWidth = 1.1;
  ctx.beginPath(); ctx.moveTo(68, 422); ctx.quadraticCurveTo(66, 444, 76, 446); ctx.quadraticCurveTo(86, 444, 84, 422); ctx.stroke();
  ctx.fillStyle = 'rgba(120,10,30,0.75)';
  ctx.beginPath(); ctx.moveTo(67.5, 436); ctx.quadraticCurveTo(68, 445, 76, 445.5); ctx.quadraticCurveTo(84, 445, 84.5, 436); ctx.closePath(); ctx.fill();
  ctx.beginPath(); ctx.moveTo(76, 446); ctx.lineTo(76, 456); ctx.stroke();
  ctx.beginPath(); ctx.ellipse(76, 457, 6, 1.5, 0, 0, TAU); ctx.stroke();
  ctx.fillStyle = 'rgba(255,240,230,0.5)'; ctx.fillRect(70, 426, 1.2, 12);
  ctx.restore();

  // lamp: turned brass stem + base (the shade is drawn live, it glows)
  ctx.fillStyle = gLinear(110, 452, 144, 452, [[0, '#5a3a1e'], [0.4, '#d4ac6a'], [1, '#4a2e16']]);
  ctx.beginPath(); ctx.ellipse(127, 456, 17, 4, 0, 0, TAU); ctx.fill();
  ctx.fillRect(124, 428, 6, 28);
  ctx.fillStyle = 'rgba(255,240,200,0.5)'; ctx.fillRect(125, 430, 1.2, 24);

  // candle on the stand (flame is live)
  ctx.fillStyle = gLinear(184, 430, 196, 430, [[0, '#d9c2a2'], [0.5, '#f4e6d0'], [1, '#b8a084']]);
  ctx.fillRect(184, 428, 12, 30);
  ctx.fillStyle = '#e8d6bc'; ctx.beginPath(); ctx.ellipse(190, 428, 6, 1.8, 0, 0, TAU); ctx.fill();
  ctx.strokeStyle = '#2a1a10'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(190, 428); ctx.lineTo(190, 424); ctx.stroke();

  // floor-to-ceiling darkness at the far right edge, beyond the window
  ctx.fillStyle = gLinear(1240, 0, W, 0, [[0, 'rgba(0,0,0,0)'], [1, 'rgba(0,0,0,0.35)']]);
  ctx.fillRect(1240, 0, 40, H);
}

function paintWindowStatic(ctx){
  const { x, y, w, h } = WIN;
  // night sky
  ctx.fillStyle = gLinear(x, y, x, y + h, [[0, '#0b1424'], [0.6, '#16253a'], [1, '#2a2a3c']]);
  ctx.fillRect(x, y, w, h);
  // stars
  for(let i = 0; i < 46; i++){
    const sx = x + 6 + _h(i) * (w - 12), sy = y + 6 + _h(i + 99) * (h * 0.7);
    const a = 0.25 + _h(i + 7) * 0.6, r = 0.4 + _h(i + 3) * 0.9;
    ctx.fillStyle = `rgba(220,232,255,${a})`;
    ctx.beginPath(); ctx.arc(sx, sy, r, 0, TAU); ctx.fill();
  }
  // distant city: warm haze + a skyline of dark blocks with lit windows
  ctx.fillStyle = gLinear(x, y + h - 80, x, y + h, [[0, 'rgba(255,140,90,0)'], [1, 'rgba(255,130,90,0.22)']]);
  ctx.fillRect(x, y + h - 80, w, 80);
  let bx = x;
  for(let i = 0; bx < x + w; i++){
    const bw = 12 + _h(i + 40) * 22, bh = 18 + _h(i + 80) * 58;
    ctx.fillStyle = '#0a0d16'; ctx.fillRect(bx, y + h - bh, bw, bh);
    for(let wy = y + h - bh + 4; wy < y + h - 4; wy += 6){
      for(let wx = bx + 3; wx < bx + bw - 3; wx += 5){
        if(_h(wx * 3.1 + wy) > 0.78){ ctx.fillStyle = `rgba(255,${190 + (_h(wx + wy) * 50 | 0)},120,0.55)`; ctx.fillRect(wx, wy, 1.6, 2); }
      }
    }
    bx += bw + 1;
  }
  // moon: disc, soft terminator, maria, corona
  const mx = 1172, my = 120;
  ctx.save(); ctx.globalCompositeOperation = 'screen';
  ctx.fillStyle = gRadial(mx, my, 14, mx, my, 90, [[0, 'rgba(200,220,245,0.30)'], [0.4, 'rgba(160,190,230,0.08)'], [1, 'rgba(0,0,0,0)']]);
  ctx.beginPath(); ctx.arc(mx, my, 90, 0, TAU); ctx.fill();
  ctx.restore();
  ctx.fillStyle = gRadial(mx - 6, my - 6, 2, mx, my, 21, [[0, '#f6f8fc'], [0.7, '#dfe6f0'], [1, '#b9c4d4']]);
  ctx.beginPath(); ctx.arc(mx, my, 20, 0, TAU); ctx.fill();
  ctx.fillStyle = 'rgba(140,152,175,0.35)';
  [[-6, -4, 5], [5, 3, 4], [-2, 8, 3], [8, -7, 2.4]].forEach(([dx, dy, r]) => { ctx.beginPath(); ctx.arc(mx + dx, my + dy, r, 0, TAU); ctx.fill(); });
  // glass reflection streak
  ctx.save(); ctx.globalCompositeOperation = 'screen';
  ctx.fillStyle = gLinear(x, y, x + w, y + h, [[0, 'rgba(255,255,255,0)'], [0.45, 'rgba(200,220,255,0.06)'], [0.5, 'rgba(200,220,255,0.0)'], [1, 'rgba(255,255,255,0)']]);
  ctx.fillRect(x, y, w, h);
  ctx.restore();
  // frame + mullions with a bevelled highlight
  ctx.strokeStyle = '#1d1116'; ctx.lineWidth = 9; ctx.strokeRect(x, y, w, h);
  ctx.strokeStyle = 'rgba(160,190,230,0.14)'; ctx.lineWidth = 1; ctx.strokeRect(x - 4, y - 4, w + 8, h + 8);
  ctx.strokeStyle = '#1d1116'; ctx.lineWidth = 5;
  ctx.beginPath();
  ctx.moveTo(x + w / 2, y); ctx.lineTo(x + w / 2, y + h);
  ctx.moveTo(x, y + h * 0.42); ctx.lineTo(x + w, y + h * 0.42);
  ctx.stroke();
  ctx.strokeStyle = 'rgba(170,200,240,0.12)'; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(x + w / 2 + 2.5, y); ctx.lineTo(x + w / 2 + 2.5, y + h); ctx.stroke();
  // sill
  ctx.fillStyle = gLinear(0, y + h + 4, 0, y + h + 16, [[0, '#3a2228'], [1, '#160b0f']]);
  ctx.fillRect(x - 16, y + h + 4, w + 32, 12);
  ctx.fillStyle = 'rgba(170,200,240,0.18)'; ctx.fillRect(x - 16, y + h + 4, w + 32, 1.2);
  // curtain rod
  ctx.fillStyle = gLinear(0, 34, 0, 42, [[0, '#d0aa6a'], [1, '#4a3016']]);
  ctx.fillRect(x - 80, 34, w + 140, 6);
  ctx.beginPath(); ctx.arc(x - 82, 37, 6, 0, TAU); ctx.arc(x + w + 62, 37, 6, 0, TAU); ctx.fill();
}

/* ============================================================
   STATIC BED (sheets, frame, folds) — blurred folds read as satin
   ============================================================ */
function paintBedStatic(ctx){
  ctx.fillStyle = '#1e0e13'; ctx.fillRect(0, 552, W, H - 552);
  // upholstered frame lip
  ctx.fillStyle = gLinear(0, 532, 0, 588, [[0, '#4a2029'], [0.35, '#32151c'], [1, '#1b090f']]);
  ctx.fillRect(24, 532, W - 48, 56);
  // sheets
  ctx.fillStyle = gLinear(0, 545, 0, 720, [[0, '#5a2b37'], [0.28, '#401d26'], [0.7, '#2c1219'], [1, '#170810']]);
  ctx.fillRect(0, 548, W, H - 548);
  // lamp warmth pooling across the near sheets
  ctx.save(); ctx.globalCompositeOperation = 'screen';
  ctx.fillStyle = gRadial(160, 560, 10, 160, 560, 700, [[0, 'rgba(255,150,90,0.20)'], [0.5, 'rgba(160,70,50,0.07)'], [1, 'rgba(0,0,0,0)']]);
  ctx.fillRect(0, 548, W, H - 548);
  ctx.restore();
  // satin folds: soft valleys and lit crests, blurred into the cloth
  ctx.save();
  ctx.filter = 'blur(5px)';
  for(let i = 0; i < 9; i++){
    const y = 566 + i * 18 + _h(i) * 8;
    const x0 = 20 + _h(i + 5) * 140, x1 = 1260 - _h(i + 9) * 160;
    const c1 = 380 + _h(i + 2) * 200, c2 = 760 + _h(i + 4) * 200;
    const dy1 = -22 + _h(i + 6) * 30, dy2 = -8 + _h(i + 8) * 26;
    ctx.lineCap = 'round';
    ctx.strokeStyle = 'rgba(10,2,6,0.38)'; ctx.lineWidth = 7 + _h(i + 1) * 6;
    ctx.beginPath(); ctx.moveTo(x0, y + 3); ctx.bezierCurveTo(c1, y + dy1 + 3, c2, y + dy2 + 3, x1, y + 2); ctx.stroke();
    ctx.strokeStyle = `rgba(255,196,170,${0.06 + _h(i + 3) * 0.06})`; ctx.lineWidth = 4 + _h(i + 2) * 4;
    ctx.beginPath(); ctx.moveTo(x0 + 20, y - 4); ctx.bezierCurveTo(c1, y + dy1 - 4, c2, y + dy2 - 4, x1 - 30, y - 5); ctx.stroke();
  }
  ctx.filter = 'none';
  // a few crisp specular glints along the crests (satin sheen)
  ctx.globalCompositeOperation = 'screen';
  for(let i = 0; i < 6; i++){
    const y = 566 + i * 26 + _h(i) * 8, x = 300 + _h(i + 30) * 700;
    shade(x, y - 3, 90 + _h(i) * 80, 2.2, 'rgba(255,214,190,0.20)', -0.02);
  }
  ctx.restore();
  // sheet hem falling off the near edge
  ctx.fillStyle = gLinear(0, 700, 0, 720, [[0, 'rgba(0,0,0,0)'], [1, 'rgba(0,0,0,0.5)']]);
  ctx.fillRect(0, 700, W, 20);
}

/* ============================================================
   LIVE ROOM PASS
   ============================================================ */
function drawRoom(){
  const t = G.t || 0;
  // lamp flicker: slow breathing plus the odd filament shimmer
  ATM.flick = 0.94 + 0.04 * Math.sin(t * 1.7) + 0.02 * Math.sin(t * 13.3 + Math.sin(t * 3.1) * 2);
  _blit(ATM.room);
  drawCurtains(t);
  drawMoonbeam(t);
  drawLampLive(t);
  drawCandle(t);
}

function drawCurtains(t){
  const { x, y, w, h } = WIN;
  const sway = Math.sin(t * 0.45) * 6 + Math.sin(t * 1.13) * 2;
  // heavy velvet drapes at both sides
  [[x - 70, 1], [x + w + 8, -1]].forEach(([cx, side], k) => {
    const s = sway * (k ? 0.7 : 1);
    X.save();
    X.beginPath();
    X.moveTo(cx, 40);
    X.lineTo(cx + 62, 40);
    X.bezierCurveTo(cx + 66 + s * side, 160, cx + 54 + s, 300, cx + 70 + s * 1.4, 400);
    X.lineTo(cx - 6 + s, 400);
    X.bezierCurveTo(cx + 4, 300, cx - 4, 160, cx, 40);
    X.closePath();
    X.fillStyle = gLinear(cx, 0, cx + 62, 0, [[0, '#1c0a10'], [0.25, '#3a1520'], [0.5, '#220c13'], [0.75, '#401823'], [1, '#1a080e']]);
    X.fill();
    X.clip();
    // moon rim on the edge facing the glass
    X.globalCompositeOperation = 'screen';
    const rx = side > 0 ? cx + 58 : cx + 4;
    X.fillStyle = gLinear(rx - 10, 0, rx + 10, 0, [[0, 'rgba(150,180,230,0)'], [0.5, 'rgba(150,180,230,0.16)'], [1, 'rgba(150,180,230,0)']]);
    X.fillRect(rx - 10, 40, 20, 360);
    X.restore();
  });
  // sheer inner curtain drifting in the draft
  X.save(); X.globalCompositeOperation = 'screen';
  for(let c = 0; c < 6; c++){
    const cx0 = x + 6 + c * 12, ph = t * 0.7 + c * 0.9;
    const d = Math.sin(ph) * 5 + sway * 0.4;
    X.fillStyle = `rgba(190,205,235,${0.035 + (c % 2) * 0.02})`;
    X.beginPath();
    X.moveTo(cx0, 42);
    X.bezierCurveTo(cx0 + 10 + d, 140, cx0 - 6 + d, 260, cx0 + 6 + d * 1.5, y + h + 2);
    X.lineTo(cx0 + 18 + d * 1.5, y + h + 2);
    X.bezierCurveTo(cx0 + 6 + d, 260, cx0 + 22 + d, 140, cx0 + 12, 42);
    X.closePath(); X.fill();
  }
  X.restore();
}

function drawMoonbeam(t){
  // a cool shaft from the window falling across the foot of the bed; nested
  // polygons give it a soft penumbra instead of a hard-edged wedge
  X.save(); X.globalCompositeOperation = 'screen';
  const beam = k => {
    X.beginPath();
    X.moveTo(WIN.x + 10 + k * 8, WIN.y + 24 + k * 4); X.lineTo(WIN.x + WIN.w - 8 - k * 6, WIN.y + 10 + k * 4);
    X.lineTo(1050 - k * 40, 720); X.lineTo(660 + k * 50, 720);
    X.closePath();
  };
  const breathe = 0.85 + 0.15 * Math.sin(t * 0.4);
  for(let k = 0; k < 4; k++){
    beam(k);
    X.fillStyle = gLinear(1130, 80, 860, 720, [[0, `rgba(150,180,235,${(0.035 * breathe).toFixed(3)})`], [0.65, 'rgba(120,150,215,0.012)'], [1, 'rgba(120,150,215,0)']]);
    X.fill();
  }
  beam(0); X.clip();
  // motes drifting through the shaft
  for(let i = 0; i < 34; i++){
    const sp = 0.012 + _h(i) * 0.02;
    const k = (_h(i + 50) + t * sp) % 1;
    const y = 90 + k * 600;
    const bx = lerp(1120, 860, k) + (_h(i + 9) - 0.5) * 300 + Math.sin(t * 0.6 + i) * 14;
    const a = (0.25 + 0.5 * _h(i + 3)) * (0.6 + 0.4 * Math.sin(t * 1.3 + i * 2.1)) * (1 - k * 0.7);
    X.fillStyle = `rgba(210,225,255,${a * 0.5})`;
    X.beginPath(); X.arc(bx, y, 0.7 + _h(i + 11) * 1.3, 0, TAU); X.fill();
  }
  X.restore();
}

function drawLampLive(t){
  const lx = 127, ly = 405, f = ATM.flick;
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = gRadial(lx, ly, 6, lx, ly, 400, [[0, `rgba(255,195,125,${0.42 * f})`], [0.25, `rgba(255,165,95,${0.18 * f})`], [0.7, 'rgba(255,140,80,0.04)'], [1, 'rgba(255,140,80,0)']]);
  X.beginPath(); X.arc(lx, ly, 400, 0, TAU); X.fill();
  // up/down light cones spilling out of the shade
  X.fillStyle = gLinear(0, 300, 0, 390, [[0, 'rgba(255,190,120,0)'], [1, `rgba(255,190,120,${0.10 * f})`]]);
  X.beginPath(); X.moveTo(110, 390); X.lineTo(144, 390); X.lineTo(200, 260); X.lineTo(54, 260); X.closePath(); X.fill();
  X.restore();
  // silk shade, lit from inside
  X.beginPath(); X.moveTo(94, 432); X.lineTo(160, 432); X.lineTo(146, 390); X.lineTo(108, 390); X.closePath();
  X.fillStyle = gLinear(94, 390, 160, 432, [[0, `rgba(255,214,150,${0.95 * f})`], [0.55, `rgba(255,176,104,${0.90 * f})`], [1, `rgba(214,110,70,${0.85 * f})`]]);
  X.fill();
  X.strokeStyle = 'rgba(255,230,190,0.5)'; X.lineWidth = 1; X.stroke();
  X.fillStyle = 'rgba(80,30,20,0.25)';
  for(let i = 0; i < 5; i++){ X.fillRect(102 + i * 12, 392, 1, 39); }
  // hot bottom rim
  X.fillStyle = `rgba(255,240,200,${0.8 * f})`; X.fillRect(95, 430, 64, 2);
}

function drawCandle(t){
  const x = 190, y = 418;
  const w = Math.sin(t * 9.1) * 0.8 + Math.sin(t * 23.7) * 0.4;
  const hgt = 1 + Math.sin(t * 6.3) * 0.08 + Math.sin(t * 17.9) * 0.05;
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = gRadial(x, y, 1, x, y, 70, [[0, 'rgba(255,190,110,0.30)'], [0.4, 'rgba(255,150,80,0.08)'], [1, 'rgba(0,0,0,0)']]);
  X.beginPath(); X.arc(x, y, 70, 0, TAU); X.fill();
  X.translate(x, y + 5); X.scale(1, hgt);
  X.beginPath();
  X.moveTo(0, 0);
  X.bezierCurveTo(-4.5, -3, -3 + w, -9, w * 1.6, -15);
  X.bezierCurveTo(3 + w, -9, 4.5, -3, 0, 0);
  X.fillStyle = gLinear(0, 0, 0, -15, [[0, 'rgba(120,150,255,0.8)'], [0.25, 'rgba(255,240,200,1)'], [1, 'rgba(255,170,80,0.7)']]);
  X.fill();
  X.restore();
}

/* ============================================================
   BED (static cache + live weight shadows + pillow)
   ============================================================ */
function drawBed(){
  _blit(ATM.bed);
  const sink = Math.sin((G.t || 0) * 8) * (G.impact || 0) * 3;
  // moonbeam pool on the sheets
  X.save(); X.globalCompositeOperation = 'screen';
  shade(900, 650, 220, 60, 'rgba(130,160,220,0.07)', -0.1);
  X.restore();

  // pillow: sits in her head's shadow; only its lamp-side rim glows
  const px = 286, py = 542;
  X.fillStyle = gRadial(px - 52, py - 18, 8, px, py, 120, [[0, '#76604c'], [0.40, '#5c493d'], [0.74, '#41322a'], [0.93, 'rgba(38,29,25,0.88)'], [1, 'rgba(38,29,25,0)']]);
  X.beginPath(); X.ellipse(px, py, 116, 36, -0.04, 0, TAU); X.fill();
  X.save(); X.globalCompositeOperation = 'screen';
  shade(px - 62, py - 16, 62, 16, 'rgba(255,196,140,0.24)', -0.12);
  X.restore();
  skArc(px - 96, py + 12, px - 40, py + 24, px + 24, py + 20, 5.5, '26,18,16', 0.30, { n: 9, thin: 0.6 });
  skArc(px + 18, py + 22, px + 62, py + 18, px + 96, py + 8, 5.0, '26,18,16', 0.24, { n: 9, thin: 0.6 });
  X.save(); X.globalCompositeOperation = 'multiply';
  shade(px + 45, py + 4, 48, 22, 'rgba(38,24,18,0.62)', -0.08);
  X.restore();

  // contact occlusion pools beneath resting bodies (follow the impact sink)
  if(G.state !== 'intro'){
    X.save(); X.globalCompositeOperation = 'multiply';
    shade(540, 580 + sink * 0.5, 360, 34, 'rgba(15,4,8,0.62)', 0);
    shade(880, 594 + sink * 0.3, 150, 24, 'rgba(15,4,8,0.50)', 0);
    shade(360, 574, 90, 20, 'rgba(15,4,8,0.45)', 0);
    X.restore();
  }
}

/* ============================================================
   SCREEN-SPACE POST: lamp bloom, arousal grade, true bloom,
   heartbeat vignette, climax flash, grain, pose crossfade
   ============================================================ */
function _grainTile(){
  if(ATM.grain) return ATM.grain;
  const c = _atmCanvas(192, 192), g = c.getContext('2d');
  const im = g.createImageData(192, 192);
  for(let i = 0; i < im.data.length; i += 4){
    const v = 128 + (Math.random() - 0.5) * 255 * 0.9;
    im.data[i] = im.data[i + 1] = im.data[i + 2] = v; im.data[i + 3] = 255;
  }
  g.putImageData(im, 0, 0);
  ATM.grain = X.createPattern(c, 'repeat');
  return ATM.grain;
}

function drawLight(){
  if(!ATM.small) return;
  const ar = G.ar || 0, t = G.t || 0, p = clamp(ar / 100, 0, 1);

  // warm lamp spill over everything (the room is lit by one practical)
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = gRadial(127, 400, 30, 127, 400, 680, [[0, `rgba(255,185,115,${0.16 * ATM.flick})`], [0.45, 'rgba(255,155,90,0.05)'], [1, 'rgba(255,155,90,0)']]);
  X.fillRect(0, 0, W, H);
  X.restore();

  // bloom: downsample, cube the luminance to isolate highlights, blur, add back
  try {
    const s = ATM.small.getContext('2d'), s2 = ATM.small2.getContext('2d');
    s.globalCompositeOperation = 'copy';
    s.drawImage(CV, 0, 0, 320, 180);
    s.globalCompositeOperation = 'multiply';
    s.drawImage(ATM.small, 0, 0); s.drawImage(ATM.small, 0, 0); s.drawImage(ATM.small, 0, 0);
    s2.globalCompositeOperation = 'copy';
    s2.filter = 'blur(5px)';
    s2.drawImage(ATM.small, 0, 0);
    s2.filter = 'none';
    X.save(); X.setTransform(1, 0, 0, 1, 0, 0);
    X.globalCompositeOperation = 'screen';
    X.globalAlpha = clamp((G.view === 'fpv' ? 0.3 : 0.6) + p * 0.3 + (G.bloom || 0) * 1.5, 0, 1);
    X.drawImage(ATM.small2, 0, 0, CV.width, CV.height);
    X.restore();
  } catch(_) {}

  // colour grade: cooler and steadier at rest, flushing rose as she builds
  X.save(); X.globalCompositeOperation = 'soft-light';
  X.fillStyle = `rgba(${Math.round(lerp(120, 255, p))},${Math.round(lerp(110, 70, p))},${Math.round(lerp(150, 110, p))},${0.10 + p * 0.16})`;
  X.fillRect(0, 0, W, H);
  X.restore();

  // arousal: a warm rush creeping in from the periphery
  if(ar > 50 && G.state !== 'intro'){
    const k = ((ar - 50) / 50) * (0.65 + 0.35 * Math.sin(t * 3.8));
    X.fillStyle = gRadial(640, 420, 220, 640, 420, 780, [[0, 'rgba(255,70,110,0)'], [0.7, `rgba(255,45,95,${(0.07 * k).toFixed(3)})`], [1, `rgba(220,25,80,${(0.22 * k).toFixed(3)})`]]);
    X.fillRect(0, 0, W, H);
  }

  // vignette that tightens with a heartbeat near the edge
  const hb = ar > 80 ? Math.pow(Math.max(0, Math.sin(t * TAU * (1.0 + p * 0.5))), 12) * (ar - 80) / 20 : 0;
  X.fillStyle = gRadial(640, 380, 330 - hb * 40, 640, 420, 880, [[0, 'rgba(6,2,6,0)'], [0.68, `rgba(6,2,6,${(0.32 + hb * 0.12).toFixed(3)})`], [1, `rgba(6,2,6,${(0.78 + hb * 0.1).toFixed(3)})`]]);
  X.fillRect(0, 0, W, H);

  // climax: white bloom washes in, with a faint chromatic halo at the rim
  const bl = G.bloom || 0;
  if(bl > 0.005){
    X.save(); X.globalCompositeOperation = 'screen';
    X.fillStyle = `rgba(255,236,232,${bl * 0.45})`; X.fillRect(0, 0, W, H);
    X.fillStyle = gRadial(640, 360, 200, 640, 360, 760, [[0, 'rgba(0,0,0,0)'], [1, `rgba(255,120,170,${bl * 0.35})`]]);
    X.fillRect(0, 0, W, H);
    X.restore();
  }

  // pose / mode crossfade: dissolve the previous frame over the new one
  if(ATM.ghostA > 0.01){
    const a = 1 - (performance.now() - ATM.ghostT0) / 420;
    ATM.ghostA = Math.max(0, a);
    if(a > 0) _blit(ATM.ghost, a * a);
  }

  // film grain (moves every frame; overlay keeps it out of the blacks)
  X.save(); X.globalCompositeOperation = 'overlay'; X.globalAlpha = 0.07;
  X.translate((Math.random() * 192) | 0, (Math.random() * 192) | 0);
  X.fillStyle = _grainTile();
  X.fillRect(-192, -192, W + 192, H + 192);
  X.restore();
}

// Called at the top of draw(): when the scene setup changes (pose, oral, solo,
// view) keep the last presented frame and dissolve it over the new one.
function atmCrossfadeCheck(){
  const sig = (G.pos | 0) + '|' + (G.oral > 0.5 ? 1 : 0) + '|' + (G.solo ? 1 : 0) + '|' + (G.fpvFocus || '');
  if(ATM.sig && sig !== ATM.sig && ATM.ghost && G.state !== 'intro'){
    const g = ATM.ghost.getContext('2d');
    g.globalCompositeOperation = 'copy';
    g.drawImage(CV, 0, 0);
    ATM.ghostA = 1; ATM.ghostT0 = performance.now();
  }
  ATM.sig = sig;
}

/* ============================================================
   BODY LAYER: characters are painted onto a cleared canvas so they can be
   composited as one unit — a soft contact shadow that grounds them on the
   sheets, then a warm lamp rim on the upper-left edges and a cool moon rim
   on the right edges. Rims computed on the merged silhouette never show the
   seams between individual limb shapes.
   ============================================================ */
function _atmLayer(name){
  if(!ATM[name] || ATM[name].width !== CV.width || ATM[name].height !== CV.height) ATM[name] = _atmCanvas(CV.width, CV.height);
  return ATM[name];
}
function _atmLayer2(name, w, h){
  if(!ATM[name]) ATM[name] = _atmCanvas(w, h);
  return ATM[name];
}
function atmBodyBegin(){
  const bg = _atmLayer('bg').getContext('2d');
  bg.globalCompositeOperation = 'copy'; bg.drawImage(CV, 0, 0);
  X.save(); X.setTransform(1, 0, 0, 1, 0, 0); X.clearRect(0, 0, CV.width, CV.height); X.restore();
}
function _rim(body, dx, dy, col, alpha){
  const rc = _atmLayer('rim'), r = rc.getContext('2d');
  r.globalCompositeOperation = 'copy'; r.drawImage(body, 0, 0);
  r.globalCompositeOperation = 'destination-out'; r.drawImage(body, dx, dy);
  r.globalCompositeOperation = 'source-in'; r.fillStyle = col; r.fillRect(0, 0, rc.width, rc.height);
  X.globalCompositeOperation = 'screen'; X.globalAlpha = alpha;
  X.drawImage(rc, 0, 0);
}
function atmBodyEnd(){
  const body = _atmLayer('body'), bc = body.getContext('2d');
  bc.globalCompositeOperation = 'copy'; bc.drawImage(CV, 0, 0);
  const k = CV.width / W;   // logical px -> device px
  X.save(); X.setTransform(1, 0, 0, 1, 0, 0);
  X.globalCompositeOperation = 'copy'; X.drawImage(ATM.bg, 0, 0);
  // contact shadow: the silhouette, darkened and blurred at quarter res
  // (cheap on any GPU), cast down-right away from the lamp
  const sh = _atmLayer2('shadow', 320, 180), sc = sh.getContext('2d');
  sc.globalCompositeOperation = 'copy';
  sc.filter = 'brightness(0) blur(2.5px)';
  sc.drawImage(body, 0, 0, 320, 180);
  sc.filter = 'none';
  X.globalCompositeOperation = 'source-over';
  X.globalAlpha = 0.55;
  X.drawImage(sh, 6 * k, 9 * k, CV.width, CV.height);
  X.globalAlpha = 1;
  X.drawImage(body, 0, 0);
  _rim(body, 2.4 * k, 2.0 * k, '#ffc690', 0.34);   // lamp, upper-left
  _rim(body, -2.2 * k, 0.6 * k, '#9fbfff', 0.22);  // moon, right
  X.restore();
}
