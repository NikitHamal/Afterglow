// Afterglow — module: chars (loaded by index.html)
'use strict';
/* ============================================================
   CHARACTER PRESETS & PALETTE SYSTEM
   G.char holds all customization traits; getSkin() derives
   canvas colors from skinTone [0..1]. lsLoad/Save persist.
   ============================================================ */

/* ---- colour helpers ---- */
function hexToRgb(h){
  h=h.replace('#','');
  const n=parseInt(h,16);
  return [(n>>16)&255,(n>>8)&255,n&255];
}
function rgbToHex(r,g,b){
  return '#'+[r,g,b].map(v=>('0'+Math.max(0,Math.min(255,v|0)).toString(16)).slice(-2)).join('');
}
function lerpHex(a,b,t){
  const [ar,ag,ab]=hexToRgb(a), [br,bg,bb]=hexToRgb(b);
  return rgbToHex(ar+(br-ar)*t, ag+(bg-ag)*t, ab+(bb-ab)*t);
}
function lerpHex3(a,m,b,t){
  return t<.5 ? lerpHex(a,m,t*2) : lerpHex(m,b,(t-.5)*2);
}

/* ---- presets ---- */
const CHARS={
  vivian:{
    preset:'vivian', name:'Vivian',
    quote:'\u201cMm\u2026 I\u2019ve waited all week for this. Close the door\u2026 and let me show you exactly how I like it.\u201d',
    skinTone:.55, hairColor:'#3a2117', hairStyle:'long',
    bodyScale:.62, breastSize:.72,
    nippleColor:'#7a4038', blushColor:'#b85a5a', lipColor:'#7a1a2a', eyeColor:'#5a3a1e',
    pubicHair:'trim', isGLB:true, glbPath:'assets/vivian/vivian.glb'
  },
};

/* ---- skin palette ---- */
// 3 stops: pale ivory → warm olive → deep brown
const _skinBase =['#f4cba8','#d4936a','#7a4028'];
const _skinSh   =['#cf9273','#a96840','#582815'];
const _skinDk   =['#c98a6d','#9e6035','#4e2010'];
const _himBase  =['#d9a176','#bd7f4e','#7a4828'];
const _himSh    =['#ad7450','#94613e','#5a3018'];

function getSkin(){
  const t=G.char?G.char.skinTone:.18;
  return {
    her:  lerpHex3(_skinBase[0], _skinBase[1], _skinBase[2], t),
    herSh:lerpHex3(_skinSh[0],  _skinSh[1],  _skinSh[2],  t),
    herDk:lerpHex3(_skinDk[0],  _skinDk[1],  _skinDk[2],  t),
    him:  lerpHex3(_himBase[0], _himBase[1], _himBase[2],  t*.62+.06),
    himSh:lerpHex3(_himSh[0],   _himSh[1],   _himSh[2],   t*.62+.06)
  };
}

/* ---- localStorage ---- */
const CHAR_KEY='ag_char';
function applyPreset(key){
  const p=CHARS[key]||CHARS.vivian;
  const him=G.char.him;                       // his look is independent of which woman is picked
  for(const k in G.char) delete G.char[k];
  Object.assign(G.char, JSON.parse(JSON.stringify(p)));
  if(him) G.char.him=him;
  lsSaveChar();
  if(typeof updatePlName==='function') updatePlName();
}
function lsSaveChar(){ try{ localStorage.setItem(CHAR_KEY, JSON.stringify(G.char)); }catch(_){} }
function loadCharState(){
  try{
    const raw=localStorage.getItem(CHAR_KEY);
    if(raw){
      const saved=JSON.parse(raw);
      // retired presets (removed from the roster) fall back to Yuki
      if(saved && saved.preset && !CHARS[saved.preset]) applyPreset('vivian');
      else Object.assign(G.char, saved);
    }
  }catch(_){}
  if(typeof updatePlName==='function') updatePlName();
}
