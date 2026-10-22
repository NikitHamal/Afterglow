// Afterglow — module: juice (loaded last, after mechanics/ambience)
'use strict';
/* ============================================================
   INTERACTION FEEL
   Watches G for edges (sweet-spot hit, combo tiers, pose change,
   orgasm, climax, spurts, exhaustion) and answers each with a
   matched popup / ring / sound / haptic, so the game reacts to
   every good input. Also: UI hover+click sounds, pause on blur,
   and a keyboard help overlay (H or ?).
   Everything is DOM-guarded: missing elements just no-op.
   ============================================================ */
const JZ = {
  layer: null, help: null, pause: null,
  px: null, py: null, touch: false,
  prev: null, streak: 0, comboTier: 0, popCd: 0,
  shot: /[?&](shot|pose)=/.test(location.search)
};
const STG = typeof stage !== 'undefined' ? stage : document.getElementById('stage');

(function juiceInit(){
  if(!STG) return;
  const css = document.createElement('style');
  css.textContent = `
#juiceLayer{position:absolute;inset:0;pointer-events:none;overflow:hidden;z-index:40}
.jPop{position:absolute;transform:translate(-50%,-50%);font:600 1.5em/1 'Sora',system-ui,sans-serif;
  letter-spacing:.14em;color:#ffe3ec;white-space:nowrap;
  text-shadow:0 0 .6em rgba(255,90,140,.85),0 .08em .2em rgba(0,0,0,.6);
  animation:jPop 1.1s cubic-bezier(.2,.9,.25,1) forwards}
.jPop.big{font-size:2.6em;color:#fff;font-family:'Cormorant Garamond',serif;font-style:italic;letter-spacing:.06em}
.jPop.gold{color:#ffe7b0;text-shadow:0 0 .7em rgba(255,180,80,.9),0 .08em .2em rgba(0,0,0,.6)}
.jPop.miss{font-size:1.05em;color:#c9b3bd;text-shadow:0 .08em .2em rgba(0,0,0,.6);letter-spacing:.2em}
@keyframes jPop{0%{opacity:0;transform:translate(-50%,-30%) scale(.6)}
  14%{opacity:1;transform:translate(-50%,-50%) scale(1.12)}
  28%{transform:translate(-50%,-55%) scale(1)}
  100%{opacity:0;transform:translate(-50%,-160%) scale(.96)}}
.jRing{position:absolute;width:10px;height:10px;margin:-5px 0 0 -5px;border-radius:50%;
  border:2px solid rgba(255,120,170,.95);box-shadow:0 0 18px rgba(255,90,150,.8),inset 0 0 12px rgba(255,140,190,.6);
  animation:jRing .7s cubic-bezier(.15,.8,.3,1) forwards}
.jRing.gold{border-color:rgba(255,210,140,.95);box-shadow:0 0 22px rgba(255,170,80,.85)}
@keyframes jRing{0%{opacity:1;transform:scale(.4)}100%{opacity:0;transform:scale(var(--s,14))}}
.jBump{animation:jBump .35s cubic-bezier(.2,1.6,.4,1)}
@keyframes jBump{0%{transform:scale(1)}40%{transform:scale(1.28)}100%{transform:scale(1)}}
#jHelp,#jPause{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;z-index:60;
  background:radial-gradient(ellipse at center,rgba(20,6,12,.72),rgba(6,2,4,.9));backdrop-filter:blur(6px);
  opacity:0;pointer-events:none;transition:opacity .25s ease}
#jHelp.on,#jPause.on{opacity:1;pointer-events:auto}
#jHelp .jCard{min-width:26em;max-width:90%;padding:2em 2.4em;border:1px solid rgba(255,150,180,.22);border-radius:1.2em;
  background:linear-gradient(160deg,rgba(40,14,24,.92),rgba(18,6,12,.94));color:#f3dfe6;font:400 1.15em/1.5 'Sora',system-ui,sans-serif;
  box-shadow:0 2em 5em rgba(0,0,0,.6),inset 0 1px 0 rgba(255,255,255,.05)}
#jHelp h2{margin:0 0 .8em;font:italic 600 2em/1 'Cormorant Garamond',serif;color:#ffd6e2;letter-spacing:.03em}
#jHelp .jGrid{display:grid;grid-template-columns:auto 1fr;gap:.45em 1.2em;align-items:center}
#jHelp kbd{display:inline-block;min-width:1.6em;padding:.15em .5em;border-radius:.4em;text-align:center;
  background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.18);border-bottom-width:2px;font:600 .85em 'Sora',monospace;color:#fff}
#jHelp .jFoot,#jPause .jFoot{margin-top:1.2em;font-size:.8em;opacity:.6;letter-spacing:.12em;text-transform:uppercase;text-align:center}
#jPause .jCard{text-align:center;color:#f3dfe6;font-family:'Sora',system-ui,sans-serif}
#jPause h2{margin:0;font:italic 600 3.4em/1 'Cormorant Garamond',serif;color:#ffd6e2;
  animation:jBreathe 3.2s ease-in-out infinite}
@keyframes jBreathe{0%,100%{opacity:.75;transform:scale(1)}50%{opacity:1;transform:scale(1.03)}}`;
  document.head.appendChild(css);

  JZ.layer = document.createElement('div'); JZ.layer.id = 'juiceLayer';
  STG.appendChild(JZ.layer);

  const row = (k, d) => `<div>${k}</div><div>${d}</div>`;
  JZ.help = document.createElement('div'); JZ.help.id = 'jHelp';
  JZ.help.innerHTML = `<div class="jCard"><h2>How to play</h2><div class="jGrid">
${row('<kbd>Space</kbd>', 'hold to thrust at her tempo')}
${row('drag <kbd>↑</kbd><kbd>↓</kbd>', 'manual control: down = deeper, up = pull back')}
${row('<kbd>E</kbd>', 'embrace / kiss')}
${row('<kbd>C</kbd> <kbd>Q</kbd> <kbd>1</kbd>–<kbd>4</kbd>', 'rub her: next spot · cycle · pick a spot (double-tap C stops)')}
${row('<kbd>Tab</kbd>', 'change position')}
${row('<kbd>O</kbd> <kbd>S</kbd>', 'oral · solo showcase')}
${row('<kbd>T</kbd>', 'turn her head')}
${row('<kbd>X</kbd>', 'climax (then mash click / Space)')}
${row('<kbd>V</kbd> <kbd>Z</kbd>', 'first-person view · zoom focus')}
${row('<kbd>P</kbd>', 'customize her')}
${row('<kbd>M</kbd> <kbd>F</kbd>', 'mute · fullscreen')}
${row('<kbd>H</kbd> <kbd>?</kbd>', 'this help')}
</div><div class="jFoot">press any key or click to close</div></div>`;
  STG.appendChild(JZ.help);
  JZ.help.addEventListener('pointerdown', e => { e.stopPropagation(); toggleHelp(false); });

  JZ.pause = document.createElement('div'); JZ.pause.id = 'jPause';
  JZ.pause.innerHTML = `<div class="jCard"><h2>paused…</h2><div class="jFoot">she's waiting · click or press any key</div></div>`;
  STG.appendChild(JZ.pause);
  JZ.pause.addEventListener('pointerdown', e => { e.stopPropagation(); setPaused(false); });

  // pointer position (for popups) + input type (for haptics)
  STG.addEventListener('pointerdown', e => { JZ.touch = e.pointerType === 'touch'; jzTrack(e); }, true);
  window.addEventListener('pointermove', jzTrack, { passive: true });

  // hover ticks + click sounds on every interactive control
  const SEL = 'button, .abtn, .ubtn, .bigbtn, .cCard, .linkbtn, kbd[data-key]';
  let lastHover = null, lastHoverT = 0;
  document.addEventListener('pointerover', e => {
    const b = e.target.closest && e.target.closest(SEL);
    if(!b || b === lastHover) return;
    lastHover = b;
    const now = performance.now();
    if(now - lastHoverT > 45 && e.pointerType !== 'touch'){ lastHoverT = now; if(typeof uiTick === 'function') uiTick(); }
  });
  document.addEventListener('pointerout', e => { if(lastHover && !lastHover.contains(e.relatedTarget)) lastHover = null; });
  document.addEventListener('click', e => {
    if(e.target.closest && e.target.closest(SEL) && typeof uiClick === 'function') uiClick();
  }, true);

  // H / ? help, any key closes overlays. Capture phase so the game never
  // sees a keypress that was only meant to dismiss an overlay.
  window.addEventListener('keydown', e => {
    if(JZ.help.classList.contains('on')){ if(e.code !== 'ShiftLeft' && e.code !== 'ShiftRight'){ e.preventDefault(); e.stopImmediatePropagation(); toggleHelp(false); } return; }
    if(G.paused){ e.preventDefault(); e.stopImmediatePropagation(); setPaused(false); return; }
    if(e.code === 'KeyH' || e.key === '?'){ e.preventDefault(); e.stopImmediatePropagation(); toggleHelp(true); }
  }, true);

  // pause when the window loses focus mid-session (not in dev shot mode)
  const autoPause = () => {
    if(JZ.shot) return;
    if(G.state === 'play' || G.state === 'orgasm' || G.state === 'climax') setPaused(true);
  };
  window.addEventListener('blur', autoPause);
  document.addEventListener('visibilitychange', () => { if(document.hidden) autoPause(); });
})();

function jzTrack(e){
  if(!STG) return;
  const r = STG.getBoundingClientRect();
  JZ.px = e.clientX - r.left; JZ.py = e.clientY - r.top;
}
/* Default anchor = where the action is: the pointer while dragging,
   otherwise a point just right of center (the bodies' contact zone). */
function jzAnchor(){
  const r = STG.getBoundingClientRect();
  if(G.dragOn && JZ.px != null) return [JZ.px, JZ.py - r.height * 0.06];
  return [r.width * 0.56, r.height * 0.42];
}
function elCenter(id){
  const el = document.getElementById(id);
  if(!el || !el.offsetParent) return null;
  const a = el.getBoundingClientRect(), r = STG.getBoundingClientRect();
  return [a.left + a.width / 2 - r.left, a.top + a.height / 2 - r.top];
}

function popText(txt, cls, x, y){
  if(!JZ.layer) return;
  if(x == null){ [x, y] = jzAnchor(); }
  // stack popups that land in the same instant instead of overprinting
  const t = performance.now();
  JZ.stack = t - (JZ.popT || 0) < 350 ? (JZ.stack || 0) + 1 : 0; JZ.popT = t;
  y -= JZ.stack * 38;
  const d = document.createElement('div');
  d.className = 'jPop' + (cls ? ' ' + cls : '');
  d.textContent = txt;
  d.style.left = (x + rr(-14, 14)) + 'px'; d.style.top = y + 'px';
  JZ.layer.appendChild(d);
  setTimeout(() => d.remove(), 1150);
}
function ring(x, y, cls, scale){
  if(!JZ.layer) return;
  if(x == null){ [x, y] = jzAnchor(); }
  const d = document.createElement('div');
  d.className = 'jRing' + (cls ? ' ' + cls : '');
  d.style.left = x + 'px'; d.style.top = y + 'px';
  if(scale) d.style.setProperty('--s', scale);
  JZ.layer.appendChild(d);
  setTimeout(() => d.remove(), 750);
}
function bump(id){
  const el = document.getElementById(id);
  if(!el) return;
  el.classList.remove('jBump'); void el.offsetWidth; el.classList.add('jBump');
}
function haptic(p){
  if(!JZ.touch || !navigator.vibrate) return;
  try{ navigator.vibrate(p); }catch(_){}
}

function toggleHelp(on){
  if(!JZ.help) return;
  on = on == null ? !JZ.help.classList.contains('on') : on;
  JZ.help.classList.toggle('on', on);
  if(on){ G.spaceHeld = false; G.dragOn = false; }
  if(typeof setPausedRaw === 'function') setPausedRaw(on);
}
function setPausedRaw(on){ G.paused = !!on; }
function setPaused(on){
  if(!JZ.pause) return;
  if(on){ G.spaceHeld = false; G.dragOn = false; if(STG) STG.classList.remove('drag'); }
  G.paused = !!on;
  JZ.pause.classList.toggle('on', !!on);
}

/* ---------------- per-frame edge watcher ---------------- */
const COMBO_TIERS = [1.5, 2.0, 2.5, 3.0, 3.5];
function juiceTick(dt){
  if(!JZ.layer) return;
  const p = JZ.prev;
  const now = {
    hot: G.hotFlash || 0, strokes: G.strokes || 0, inBand: G.inBand || 0,
    combo: G.combo || 1, pos: G.pos | 0, orgasms: G.orgasms || 0, state: G.state,
    spurts: G.spurts || 0, tired: !!G.tired, impact: G.impact || 0
  };
  JZ.prev = now;
  if(!p || G.paused) return;
  JZ.popCd -= dt;

  // sweet spot
  if(now.hot > 1.2 && p.hot <= 0.05){
    popText('PERFECT ♥', 'gold');
    ring(null, null, 'gold', 16);
    const hz = elCenter('hotZone'); if(hz) ring(hz[0], hz[1], null, 6);
    if(typeof uiSparkle === 'function') uiSparkle();
    haptic(18);
  }

  // rhythm streak
  if(now.strokes > p.strokes){
    if(now.inBand > p.inBand){
      JZ.streak++;
      if(JZ.streak === 4 || JZ.streak === 10 || JZ.streak % 20 === 0) popText('IN SYNC · ' + JZ.streak);
    } else {
      if(JZ.streak >= 5) popText('rhythm lost', 'miss');
      JZ.streak = 0;
    }
    if(now.impact > 1.0) haptic(9);
  }

  // combo tiers (chime climbs the scale)
  let tier = 0; while(tier < COMBO_TIERS.length && now.combo >= COMBO_TIERS[tier]) tier++;
  if(tier > JZ.comboTier){
    popText('×' + COMBO_TIERS[tier - 1].toFixed(1), tier >= 4 ? 'gold' : '');
    if(typeof uiChime === 'function') uiChime(tier + 1);
    bump('comboChip');
  }
  JZ.comboTier = tier;

  if(now.pos !== p.pos){
    if(typeof uiWhoosh === 'function') uiWhoosh();
    const r = STG.getBoundingClientRect();
    if(typeof posName === 'function') popText(posName(), 'miss', r.width / 2, r.height * 0.2);
    bump('btnPos');
  }

  if(now.orgasms > p.orgasms){
    const r = STG.getBoundingClientRect();
    popText(now.orgasms > 1 ? 'she came · ×' + now.orgasms + ' ♥' : 'she came ♥', 'big', r.width / 2, r.height * 0.36);
    ring(r.width / 2, r.height * 0.45, null, 60);
    haptic([40, 50, 70, 50, 140]);
    bump('orgHearts');
  }
  if(now.state === 'climax' && p.state !== 'climax'){
    const r = STG.getBoundingClientRect();
    popText('mash!', 'big', r.width / 2, r.height * 0.3);
    haptic(30);
  }
  if(now.spurts > p.spurts){ ring(null, null, null, 10); haptic(35); }
  if(now.state === 'finish' && p.state !== 'finish'){
    const r = STG.getBoundingClientRect();
    popText(G.sync ? 'together ♥' : 'afterglow…', 'big', r.width / 2, r.height * 0.34);
    ring(r.width / 2, r.height * 0.45, 'gold', 70);
    haptic([60, 60, 200]);
  }
  if(now.tired && !p.tired) popText('out of breath…', 'miss');
}
if(typeof window !== 'undefined'){
  Object.assign(window, { juiceTick, popText, haptic, toggleHelp, setPaused });
}
