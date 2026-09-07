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
  yuki:{
    preset:'yuki', name:'Yuki',
    quote:'\u201cMm\u2026 finally we\u2019re alone. You remember how I like it\u2026 right?\u201d',
    skinTone:.18, hairColor:'#231318', hairStyle:'long',
    bodyScale:.45, breastSize:.45,
    nippleColor:'#c25f63', blushColor:'#e86070', lipColor:'#b3555f', eyeColor:'#4a2c33',
    pubicHair:'trim'
  },
  mara:{
    preset:'mara', name:'Mara',
    quote:'\u201cHey\u2026 don\u2019t keep me waiting. Come closer\u2026\u201d',
    skinTone:.46, hairColor:'#1a0e0a', hairStyle:'long',
    bodyScale:.65, breastSize:.68,
    nippleColor:'#a04848', blushColor:'#c05858', lipColor:'#943040', eyeColor:'#3a2018',
    pubicHair:'trim'
  },
  sable:{
    preset:'sable', name:'Sable',
    quote:'\u201cYou sure you can handle this\u2026? Good.\u201d',
    skinTone:.76, hairColor:'#14080a', hairStyle:'natural',
    bodyScale:.58, breastSize:.55,
    nippleColor:'#6a2828', blushColor:'#883040', lipColor:'#6a2435', eyeColor:'#2a1208',
    pubicHair:'full'
  }
};

/* ---- skin palette ---- */
// 3 stops: pale ivory → warm olive → deep brown
const _skinBase =['#f4cba8','#d4936a','#7a4028'];
const _skinSh   =['#cf9273','#a96840','#582815'];
const _skinDk   =['#c98a6d','#9e6035','#4e2010'];
const _himBase  =['#dba06f','#c07840','#7a4828'];
const _himSh    =['#a96c44','#8a5030','#5a3018'];

function getSkin(){
  const t=G.char?G.char.skinTone:.38;
  return {
    her:  lerpHex3(_skinBase[0], _skinBase[1], _skinBase[2], t),
    herSh:lerpHex3(_skinSh[0],  _skinSh[1],  _skinSh[2],  t),
    herDk:lerpHex3(_skinDk[0],  _skinDk[1],  _skinDk[2],  t),
    him:  lerpHex3(_himBase[0], _himBase[1], _himBase[2],  t*.5+.1),
    himSh:lerpHex3(_himSh[0],   _himSh[1],   _himSh[2],   t*.5+.1)
  };
}

/* ---- localStorage ---- */
const CHAR_KEY='ag_char_sim';
function applyPreset(key){
  const p=CHARS[key]||CHARS.yuki;
  Object.assign(G.char, JSON.parse(JSON.stringify(p)));
  lsSaveChar();
  if(typeof updatePlName==='function') updatePlName();
}
function lsSaveChar(){ try{ localStorage.setItem(CHAR_KEY, JSON.stringify(G.char)); }catch(_){} }
function loadCharState(){
  try{
    const raw=localStorage.getItem(CHAR_KEY);
    if(raw){ const saved=JSON.parse(raw); Object.assign(G.char, saved); }
    else { Object.assign(G.char, JSON.parse(JSON.stringify(CHARS.yuki))); }
  }catch(_){
    Object.assign(G.char, JSON.parse(JSON.stringify(CHARS.yuki)));
  }
  if(typeof updatePlName==='function') updatePlName();
}
