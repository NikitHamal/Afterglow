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
   ADVANCED SHADING & PROCEDURAL PRIMITIVES
   ============================================================ */
function sg(y0, y1, c1, c2) {
  const ya = Number.isFinite(y0) ? y0 : 0;
  const yb = Number.isFinite(y1) ? y1 : 100;
  const g = X.createLinearGradient(0, ya, 0, yb);
  g.addColorStop(0, c1);
  g.addColorStop(1, c2);
  return g;
}

function shade(x, y, rx, ry, col, rot = 0, lc) {
  const cx = Number.isFinite(x) ? x : 0, cy = Number.isFinite(y) ? y : 0;
  const rad = Math.max(Number.isFinite(rx) ? Math.abs(rx) : 10, Number.isFinite(ry) ? Math.abs(ry) : 10) * 1.15;
  const g = X.createRadialGradient(cx, cy, 0, cx, cy, rad > 0 ? rad : 10);
  if (lc) g.addColorStop(0, lc);
  else g.addColorStop(0, col);
  g.addColorStop(1, 'rgba(0,0,0,0)');
  X.fillStyle = g;
  X.beginPath();
  X.ellipse(cx, cy, Math.max(1, Number.isFinite(rx) ? Math.abs(rx) : 10), Math.max(1, Number.isFinite(ry) ? Math.abs(ry) : 10), rot, 0, TAU);
  X.fill();
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

/* ============================================================
   ATMOSPHERE, ROOM & ENVIRONMENT
   ============================================================ */
function drawRoom() {
  // Deep ambient boudoir tones
  const wallG = X.createLinearGradient(0, 0, 0, 560);
  wallG.addColorStop(0, '#120b0e');
  wallG.addColorStop(0.65, '#180e13');
  wallG.addColorStop(1, '#201217');
  X.fillStyle = wallG;
  X.fillRect(0, 0, W, H);

  // Ceiling cornices & subtle panel molding
  X.fillStyle = 'rgba(0,0,0,0.45)';
  X.fillRect(0, 0, W, 72);
  X.fillStyle = 'rgba(255,200,160,0.02)';
  X.fillRect(0, 70, W, 2);

  // Window frame, moonlight & sheer curtain drapery
  const winX = 1030, winY = 60, winW = 200, winH = 260;
  X.fillStyle = 'rgba(135,155,175,0.07)';
  X.fillRect(winX, winY, winW, winH);

  // Moonlit night sky gradient
  const skyG = X.createLinearGradient(winX, winY, winX, winY + winH);
  skyG.addColorStop(0, 'rgba(80,105,130,0.22)');
  skyG.addColorStop(1, 'rgba(25,35,45,0.10)');
  X.fillStyle = skyG;
  X.fillRect(winX, winY, winW, winH);

  // Moon with realistic atmospheric glow corona
  const moonX = 1172, moonY = 120;
  const mCorona = X.createRadialGradient(moonX, moonY, 10, moonX, moonY, 70);
  mCorona.addColorStop(0, 'rgba(225,238,250,0.25)');
  mCorona.addColorStop(0.5, 'rgba(180,210,235,0.06)');
  mCorona.addColorStop(1, 'rgba(0,0,0,0)');
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = mCorona;
  X.beginPath(); X.arc(moonX, moonY, 70, 0, TAU); X.fill();
  X.fillStyle = 'rgba(235,244,252,0.85)';
  X.beginPath(); X.arc(moonX, moonY, 20, 0, TAU); X.fill();
  X.restore();

  // Window mullions
  X.strokeStyle = '#22151a';
  X.lineWidth = 5;
  X.strokeRect(winX, winY, winW, winH);
  X.beginPath();
  X.moveTo(winX + winW / 2, winY); X.lineTo(winX + winW / 2, winY + winH);
  X.moveTo(winX, winY + winH * 0.42); X.lineTo(winX + winW, winY + winH * 0.42);
  X.stroke();

  // Translucent folded curtains catching moonlight rim
  X.save();
  X.globalCompositeOperation = 'soft-light';
  X.fillStyle = 'rgba(200,215,230,0.18)';
  for (let c = 0; c < 5; c++) {
    const cx0 = winX - 35 + c * 8;
    X.beginPath();
    X.moveTo(cx0, 40);
    X.bezierCurveTo(cx0 + 20, 150, cx0 - 15, 260, cx0 + 12, 360);
    X.lineTo(cx0 + 22, 360);
    X.bezierCurveTo(cx0 - 5, 260, cx0 + 30, 150, cx0 + 10, 40);
    X.closePath();
    X.fill();
  }
  X.restore();

  // Dark timber nightstand with soft beveled edge
  X.fillStyle = '#1c0f13';
  X.fillRect(48, 465, 158, 120);
  const bevelG = X.createLinearGradient(48, 465, 48, 475);
  bevelG.addColorStop(0, 'rgba(255,190,130,0.16)');
  bevelG.addColorStop(1, 'rgba(0,0,0,0)');
  X.fillStyle = bevelG;
  X.fillRect(48, 465, 158, 10);
  X.strokeStyle = '#2e1820';
  X.lineWidth = 2;
  X.strokeRect(66, 488, 120, 32);
  // Brass drawer knob
  X.fillStyle = '#bfa068';
  X.beginPath(); X.arc(126, 504, 3.8, 0, TAU); X.fill();
  X.fillStyle = 'rgba(255,255,230,0.8)';
  X.beginPath(); X.arc(125, 503, 1.2, 0, TAU); X.fill();

  // Glass and warm filament bedside lamp
  const lx = 127, ly = 405;
  // Multi-pass volumetric light burst
  const lg1 = X.createRadialGradient(lx, ly, 8, lx, ly, 380);
  lg1.addColorStop(0, 'rgba(255,195,125,0.40)');
  lg1.addColorStop(0.28, 'rgba(255,170,95,0.18)');
  lg1.addColorStop(0.7, 'rgba(255,140,80,0.05)');
  lg1.addColorStop(1, 'rgba(255,140,80,0)');
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = lg1;
  X.beginPath(); X.arc(lx, ly, 380, 0, TAU); X.fill();
  X.restore();

  // Turned brass lamp stem
  X.strokeStyle = '#684525'; X.lineWidth = 5;
  X.beginPath(); X.moveTo(lx, 465); X.lineTo(lx, 428); X.stroke();
  X.strokeStyle = '#c49e62'; X.lineWidth = 2;
  X.beginPath(); X.moveTo(lx - 1, 465); X.lineTo(lx - 1, 428); X.stroke();

  // Silk empire lampshade
  X.fillStyle = '#42242b';
  X.beginPath();
  X.moveTo(94, 432); X.lineTo(160, 432); X.lineTo(146, 390); X.lineTo(108, 390);
  X.closePath(); X.fill();
  // Glowing shade interior
  const shadeG = X.createLinearGradient(108, 390, 146, 432);
  shadeG.addColorStop(0, 'rgba(255,225,160,0.85)');
  shadeG.addColorStop(0.5, 'rgba(255,185,115,0.65)');
  shadeG.addColorStop(1, 'rgba(255,140,85,0.45)');
  X.fillStyle = shadeG;
  X.beginPath();
  X.moveTo(109, 392); X.lineTo(145, 392); X.lineTo(156, 430); X.lineTo(98, 430);
  X.closePath(); X.fill();
}

function drawBed() {
  // Deep mattress base
  X.fillStyle = '#1e0e13';
  X.fillRect(0, 552, W, H - 552);

  // Luxurious mattress topper & upholstered frame
  const frameG = X.createLinearGradient(0, 532, 0, 585);
  frameG.addColorStop(0, '#421d26');
  frameG.addColorStop(0.4, '#31141c');
  frameG.addColorStop(1, '#1b090f');
  X.fillStyle = frameG;
  X.fillRect(24, 532, W - 48, 56);

  // Satin Duvet & Crumpled Bed Sheets
  const sheetG = X.createLinearGradient(0, 545, 0, 720);
  sheetG.addColorStop(0, '#532833');
  sheetG.addColorStop(0.3, '#3d1c24');
  sheetG.addColorStop(0.7, '#2c1219');
  sheetG.addColorStop(1, '#1a090e');
  X.fillStyle = sheetG;
  X.fillRect(0, 550, W, H - 550);

  // Dynamic tension wrinkles radiating from hip and knee weight
  const d = G.depth || 0;
  const sink = Math.sin((G.t || 0) * 8) * (G.impact || 0) * 3;

  X.save();
  X.globalCompositeOperation = 'multiply';
  X.strokeStyle = 'rgba(20,5,8,0.48)';
  X.lineWidth = 3.5;
  for (let i = 0; i < 7; i++) {
    const y = 562 + i * 22;
    X.beginPath();
    X.moveTo(40 + ((i * 73) % 110), y + sink * 0.4);
    X.bezierCurveTo(460, y - 24 + ((i * 31) % 28) + sink, 820, y + 14 - ((i * 17) % 20), 1240 - ((i * 47) % 120), y);
    X.stroke();
  }
  X.restore();

  // Soft satin highlight ridges catching bedside lamp
  X.save();
  X.globalCompositeOperation = 'soft-light';
  X.strokeStyle = 'rgba(255,210,185,0.24)';
  X.lineWidth = 4;
  for (let i = 0; i < 5; i++) {
    const y = 558 + i * 26;
    X.beginPath();
    X.moveTo(80 + ((i * 57) % 80), y - 4);
    X.bezierCurveTo(440, y - 30 + ((i * 27) % 24), 780, y + 8, 1180, y - 6);
    X.stroke();
  }
  X.restore();

  // Crushed Down Pillow (indented naturally beneath her head)
  const px = 286, py = 542;
  const pilG = X.createLinearGradient(px - 110, py - 35, px + 110, py + 35);
  pilG.addColorStop(0, '#c7b096');
  pilG.addColorStop(0.4, '#b0977c');
  pilG.addColorStop(0.85, '#8a7460');
  pilG.addColorStop(1, '#635142');
  X.fillStyle = pilG;
  X.beginPath();
  X.ellipse(px, py, 112, 34, -0.04, 0, TAU);
  X.fill();

  // Weight depression crater under head
  X.save();
  X.globalCompositeOperation = 'multiply';
  const cratG = X.createRadialGradient(px + 45, py + 4, 4, px + 45, py + 4, 52);
  cratG.addColorStop(0, 'rgba(75,52,38,0.55)');
  cratG.addColorStop(0.65, 'rgba(85,58,42,0.22)');
  cratG.addColorStop(1, 'rgba(0,0,0,0)');
  X.fillStyle = cratG;
  X.beginPath();
  X.ellipse(px + 45, py + 4, 48, 22, -0.08, 0, TAU);
  X.fill();
  X.restore();

  // Heavy Ambient Occlusion pools beneath resting bodies
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(540, 580 + sink * 0.5, 340, 32, 'rgba(15,4,8,0.62)', 0);
  shade(880, 594 + sink * 0.3, 140, 24, 'rgba(15,4,8,0.50)', 0);
  shade(360, 574, 90, 20, 'rgba(15,4,8,0.45)', 0);
  X.restore();
}

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

function drawLight() {
  const ar = G.ar || 0;
  const t = G.t || 0;

  // Caustic bloom from bedside warm lamp
  X.save();
  X.globalCompositeOperation = 'screen';
  const lampG = X.createRadialGradient(127, 400, 30, 127, 400, 680);
  lampG.addColorStop(0, 'rgba(255,185,115,0.18)');
  lampG.addColorStop(0.45, 'rgba(255,155,90,0.06)');
  lampG.addColorStop(1, 'rgba(255,155,90,0)');
  X.fillStyle = lampG;
  X.fillRect(0, 0, W, H);
  X.restore();

  // Arousal peripheral warm rush
  if (ar > 50 && G.state === 'play') {
    const intensity = ((ar - 50) / 50) * (0.65 + 0.35 * Math.sin(t * 3.8));
    const rushG = X.createRadialGradient(640, 420, 220, 640, 420, 780);
    rushG.addColorStop(0, 'rgba(255,70,110,0)');
    rushG.addColorStop(0.7, `rgba(255,45,95,${0.08 * intensity})`);
    rushG.addColorStop(1, `rgba(220,25,80,${0.24 * intensity})`);
    X.save();
    X.fillStyle = rushG;
    X.fillRect(0, 0, W, H);
    X.restore();
  }

  // Cinematic 35mm optical vignette
  const vigG = X.createRadialGradient(640, 380, 340, 640, 420, 880);
  vigG.addColorStop(0, 'rgba(8,3,8,0)');
  vigG.addColorStop(0.7, 'rgba(8,3,8,0.30)');
  vigG.addColorStop(1, 'rgba(8,3,8,0.72)');
  X.fillStyle = vigG;
  X.fillRect(0, 0, W, H);

  // Climax blinding white bloom
  if ((G.bloom || 0) > 0.005) {
    X.save();
    X.fillStyle = `rgba(255,242,236,${G.bloom * 0.52})`;
    X.fillRect(0, 0, W, H);
    X.restore();
  }

  // Suspended atmospheric dust motes catching beam light
  X.save();
  X.globalCompositeOperation = 'screen';
  for (let i = 0; i < 16; i++) {
    const mt = t * 0.12 + i * 4.3;
    const mx = 130 + ((i * 89) % 360) + Math.sin(mt + i) * 45;
    const my = 360 + Math.cos(mt * 0.85 + i * 2.1) * 130 + ((i * 47) % 150);
    const alpha = 0.04 + 0.03 * Math.sin(mt * 2 + i);
    X.fillStyle = `rgba(255,220,175,${alpha})`;
    X.beginPath();
    X.arc(mx, my, 1.2 + (i % 3) * 0.8, 0, TAU);
    X.fill();
  }
  X.restore();
}