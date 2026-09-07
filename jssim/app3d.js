// Afterglow 3D — module: app3d (main loop, HUD sync, camera wiring)
// Replaces js/app.js for 3d.html. Uses the SAME G state, mechanics and audio.
'use strict';

const $3 = id => document.getElementById(id);

/* ============================================================
   HUD DOM SYNC  (mirrors app.js hudTick, 3D-tuned)
   ============================================================ */
const HUD3 = {};
let lastOrg3 = -1, lastRound3 = 1;

function cacheHUD3() {
  ['plFill', 'plBar', 'plMood', 'dpFill', 'hotZone', 'dpMark', 'stamFill', 'stamBox',
    'stamVal', 'stamLbl', 'tBand', 'tMark', 'comboChip', 'clock', 'strk', 'btnClimax',
    'btnKiss', 'btnRub', 'btnPos', 'btnOral', 'btnZoom', 'btnMute', 'icoSnd'
  ].forEach(id => { HUD3[id] = $3(id); });
}

const MOODS3 = [[0, 'snug'], [22, 'blushing'], [45, 'melting'], [68, 'trembling'], [85, 'on the edge'], [99.9, 'about to break']];

function hudTick3() {
  const h = HUD3;
  if (h.plFill) h.plFill.style.transform = `scaleY(${G.pleasure / 100})`;
  if (h.plBar) h.plBar.classList.toggle('hot', G.pleasure > 82);

  let m = 'melting';
  for (const [th, l] of MOODS3) { if (G.pleasure <= th) { m = l; break; } }
  if (h.plMood) {
    h.plMood.textContent = G.state === 'intro' ? m
      : G.state === 'orgasm' ? 'climaxing'
        : G.state === 'climax' || G.state === 'finish' ? 'full of you' : m;
  }

  if (h.dpFill) h.dpFill.style.transform = `scaleY(${G.depth.toFixed(3)})`;
  const z = (typeof hotZone === 'function') ? hotZone() : { c: .55, w: .15 };
  if (h.hotZone) { h.hotZone.style.bottom = (z.c * 100) + '%'; h.hotZone.style.height = (z.w * 100) + '%'; }
  if (h.dpMark) h.dpMark.style.bottom = `calc(${(G.depth * 100).toFixed(1)}% - 1px)`;

  if (h.stamFill) h.stamFill.style.transform = `scaleX(${G.stamina / 100})`;
  if (h.stamVal) h.stamVal.textContent = Math.round(G.stamina);
  if (h.stamBox) h.stamBox.classList.toggle('tired', G.tired);
  if (h.stamLbl) h.stamLbl.textContent = G.tired ? 'catching breath' : 'stamina';

  const b = (typeof sweetBand === 'function') ? sweetBand() : { c: 1.4, hw: .2 };
  const MAXR = 3;
  if (h.tBand) {
    h.tBand.style.left = clamp((b.c - b.hw) / MAXR * 100, 0, 96) + '%';
    h.tBand.style.width = clamp(b.hw * 2 / MAXR * 100, 2, 100) + '%';
  }
  if (h.tMark) h.tMark.style.left = clamp(G.rate / MAXR * 100, 0, 99) + '%';

  if (h.comboChip) {
    const cb = h.comboChip.querySelector('b');
    if (G.combo > 1.05) { h.comboChip.classList.add('on'); if (cb) cb.textContent = G.combo.toFixed(1); }
    else h.comboChip.classList.remove('on');
  }

  const mn = (G.sesT / 60) | 0, sc = ('0' + ((G.sesT % 60) | 0)).slice(-2);
  if (h.clock) h.clock.textContent = G.round > 1 ? `R${G.round} · ${mn}:${sc}` : `${mn}:${sc}`;
  if (h.strk) h.strk.textContent = G.strokes + (G.strokes === 1 ? ' stroke' : ' strokes');

  if (h.btnClimax) {
    h.btnClimax.disabled = !(G.sesT > CFG.climaxAt && (G.state === 'play' || G.state === 'orgasm'));
    h.btnClimax.classList.toggle('ready', G.pleasure > 85 && G.state === 'play');
  }
  if (h.btnKiss) h.btnKiss.classList.toggle('on', G.kissT > 0);
  if (h.btnRub) h.btnRub.classList.toggle('on', G.rubT > 0);
  if (h.btnOral) h.btnOral.classList.toggle('on', G.oralT > 0);

  const bRl = h.btnRub;
  if (bRl) {
    const zl = 'RUB · ' + ((typeof rubZoneName === 'function' ? RUBLBL[rubZoneName()] : null) || 'BOTH') + ' <kbd>C</kbd>';
    if (bRl._zl !== zl) { bRl._zl = zl; bRl.innerHTML = zl; }
  }
  const bPl = h.btnPos;
  if (bPl) {
    const pl = 'POS · ' + ((typeof posName === 'function') ? posName() : 'MISSIONARY') + ' <kbd>Tab</kbd>';
    if (bPl._pl !== pl) { bPl._pl = pl; bPl.innerHTML = pl; }
  }
  if (h.btnZoom) h.btnZoom.classList.toggle('on', (CAM3.mode === 'fpv' || (typeof G !== 'undefined' && G.view === 'fpv')) && G.fpvFocus !== 'full');

  // speech bubble — park it above her head in 3D
  const bub = $3('bubble');
  if (bub) {
    if (G.speech) {
      bub.textContent = G.speech.txt;
      bub.classList.add('show');
      bub.style.left = '50%';
      bub.style.transform = 'translateX(-50%)';
      bub.style.top = '9%';
    } else bub.classList.remove('show');
  }

  if (G.orgasms !== lastOrg3) {
    lastOrg3 = G.orgasms;
    if (typeof tierHearts === 'function') tierHearts();
  }
  if (G.round !== lastRound3) { lastRound3 = G.round; clearFX3(); }
}

/* ============================================================
   CHARACTER LOOK SYNC — live customization (P panel)
   ============================================================ */
let _lookKey = '';
function syncCharLook3() {
  const c = G.char || {};
  const key = [c.preset, c.hairColor, c.skinTone, c.breastSize, c.hairStyle, c.eyeColor, c.nippleColor].join('|');
  if (key === _lookKey) return;
  _lookKey = key;
  if (!her3) return;

  // skin + hair + breast scale can be retinted live
  const sk = (typeof getSkin === 'function') ? getSkin() : null;
  if (sk) her3.mat.color.set(sk.her);
  if (her3.hairMat) her3.hairMat.color.set(c.hairColor || '#231318');
  const bsz = 0.62 + (c.breastSize == null ? 0.45 : c.breastSize) * 0.85;
  her3.breastL.scale.setScalar(bsz);
  her3.breastR.scale.setScalar(bsz);
}

/* ============================================================
   CAMERA MODE ↔ G.view
   ============================================================ */
function syncView3() {
  const want = (G.view === 'fpv') ? 'fpv' : null;
  if (want === 'fpv' && CAM3.mode !== 'fpv') { CAM3.mode = 'fpv'; syncCamButtons3(); }
  else if (!want && CAM3.mode === 'fpv') { CAM3.mode = 'orbit'; syncCamButtons3(); }

  // Z drives the FPV dolly (zoom toward/away along the gaze).
  // Yaw/pitch belong to the user (drag / arrows) and are never stomped here.
  if (CAM3.mode === 'fpv') {
    const z = G.fpvZoom == null ? 1 : G.fpvZoom;
    CAM3.fpvDist = clamp(z, 0.15, 3.2);
  }
}

/* ============================================================
   RESIZE (also feeds dragK used by js/input.js)
   ============================================================ */
function resizeAll3() {
  resize3();
  const st = $3('stage');
  const w = st ? st.clientWidth : window.innerWidth;
  dragK = 1 / (w * CFG.dragSens);
  if (st) st.style.fontSize = (w / 1280 * 10) + 'px';
}
window.addEventListener('resize', resizeAll3);

/* ============================================================
   SCREENSHOT MODE (dev pipeline — inert unless URL params present)
   ?pose=0..6 &oral=1 &clean=1 &dist=1.6 &yaw=.. &pitch=.. &depth=..
   Sets the scene, hides UI, settles physics, then flags READY in
   document.title so a headless capture script knows when to shoot.
   ============================================================ */
const SHOT3 = { on: false, frames: 0, oral: false, depth: 1, cam: null };
function shotBoot3() {
  let q = null;
  try { q = new URLSearchParams(location.search); } catch (e) { return; }
  if (!q || (!q.has('pose') && !q.has('shot'))) return;
  SHOT3.on = true;
  if (q.has('pose')) G.pos = clamp(parseInt(q.get('pose'), 10) | 0, 0, 6);
  if (q.get('oral') === '1') SHOT3.oral = true;
  if (q.get('depth') != null && q.get('depth') !== '') SHOT3.depth = clamp(parseFloat(q.get('depth')), 0, 1);
  if (typeof G !== 'undefined') {
    G.state = 'play';
    G.ar = 80; G.pleasure = 55;
    G.oralDepth = 0.7; G.oralGag = 0;
    G.rub = 0; G.kissT = 0;
    if (q.get('view') === 'fpv') G.view = 'fpv';
    if (q.has('focus')) { G.fpvFocus = q.get('focus'); SHOT3.focus = q.get('focus'); }
  }
  // stored and re-asserted every frame — pose auto-cam must never win
  SHOT3.cam = {};
  if (typeof CAM3 !== 'undefined') {
    if (q.has('dist')) SHOT3.cam.dist = parseFloat(q.get('dist'));
    if (q.has('yaw')) SHOT3.cam.yaw = parseFloat(q.get('yaw'));
    if (q.has('pitch')) SHOT3.cam.pitch = parseFloat(q.get('pitch'));
    if (q.has('tx') || q.has('ty') || q.has('tz')) {
      SHOT3.cam.target = [
        q.has('tx') ? parseFloat(q.get('tx')) : CAM3.target.x,
        q.has('ty') ? parseFloat(q.get('ty')) : CAM3.target.y,
        q.has('tz') ? parseFloat(q.get('tz')) : CAM3.target.z
      ];
    }
  }
  if (q.get('clean') === '1') {
    try {
      const css = document.createElement('style');
      css.textContent = '.hud,#camBar,#camHint,#bubble,.ov,#custPanel,#charSel{display:none!important}#bootMsg{display:none!important}';
      document.head.appendChild(css);
    } catch (e) {}
  }
}
function shotTick3() {
  if (!SHOT3.on) return;
  if (SHOT3.oral) { G.oral = 1; }
  if (SHOT3.depth != null) { G.depth = SHOT3.depth; }
  if (SHOT3.focus) { G.fpvFocus = SHOT3.focus; }
  if (SHOT3.cam) {
    if (SHOT3.cam.dist != null) CAM3.dist = SHOT3.cam.dist;
    if (SHOT3.cam.yaw != null) CAM3.yaw = SHOT3.cam.yaw;
    if (SHOT3.cam.pitch != null) CAM3.pitch = SHOT3.cam.pitch;
    if (SHOT3.cam.target) CAM3.target.set(SHOT3.cam.target[0], SHOT3.cam.target[1], SHOT3.cam.target[2]);
    // snap the smoothed state too — exact framing, no lerp drift
    CAM3._dist = CAM3.dist; CAM3._yaw = CAM3.yaw; CAM3._pitch = CAM3.pitch;
  }
  SHOT3.frames++;
  if (SHOT3.frames === 160) { try { document.title = 'SHOT-READY'; } catch (e) {} }
}

/* ============================================================
   FRAME LOOP
   ============================================================ */
let last3 = performance.now();
function tick3(ts) {
  const dt = clamp((ts - last3) / 1000, 0, 0.05);
  last3 = ts;

  update(dt);            // shared game logic (js/mechanics.js) — unchanged
  syncCharLook3();
  shotTick3();           // dev screenshot mode (no-op in normal play)
  updateAnim3(dt);
  updateFX3(dt);
  syncView3();
  updateCamera3(dt, G);
  hudTick3();

  if (typeof updateGLBModel === 'function') updateGLBModel(dt);
  if (renderer3d) renderer3d.render(scene3d, camera3d);
  requestAnimationFrame(tick3);
}

/* ============================================================
   BOOT
   ============================================================ */
function boot3() {
  if (!initEngine3()) return;      // no webgl — message already shown
  initChars3();
  initFX3();
  cacheHUD3();
  if (typeof initGLBModel === 'function') initGLBModel();

  if (typeof loadCharState === 'function') loadCharState();
  if (typeof initCharUI === 'function') initCharUI();
  if (typeof setMute === 'function' && typeof muted !== 'undefined') setMute(muted);

  // re-apply materials + rig now that the saved character is loaded
  const rebuild = () => {
    if (her3) { scene3d.remove(her3.root); }
    if (him3) { scene3d.remove(him3.root); }
    her3 = buildHer3(); him3 = buildHim3();
    scene3d.add(her3.root); scene3d.add(him3.root);
    _prevPose3 = currentPose3();
    _lookKey = '';
  };
  rebuild();

  resizeAll3();
  applyPoseCam3(true);
  shotBoot3();           // dev screenshot mode (no-op in normal play)
  requestAnimationFrame(tick3);
}

if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot3);
else boot3();
