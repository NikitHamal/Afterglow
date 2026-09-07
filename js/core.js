// Afterglow — module: core (loaded by index.html)
'use strict';
/* ============================================================
   AFTERGLOW — single-file interactive scene
   All art & audio generated procedurally. Tuning knobs are
   gathered in CFG below so you can rebalance the game easily.
   ============================================================ */
const CFG={
  dragSens:0.30,          // fraction of screen-width dragged = full depth
  strokeMinRange:0.22,   // min stroke amplitude to count
  pleaBase:1.85,         // pleasure per stroke (before modifiers)
  pleaDecay:1.35,        // %/sec lost while idle
  gainDamp:260,           // higher = harder to reach 100 near climax
  stamDrill:1.9,         // stamina drain = 0.7 + rate*drill*depth
  stamRegen:8,           // %/sec recovering while resting
  kissGain:1.28, kissCost:1.8,
  rubGain:[1.5,0.012], rubCost:2.2,   // +% /s + per-arousal bonus
  band:[1.05,1.85,0.34,0.22], // startHz,endHz,halfWstart,halfWend
  hotW:0.15,             // width of her spot
  orgBonus:38,            // stamina refund when she finishes
  orgFloor:44,           // her pleasure floor after each orgasm
  sensStep:0.22,         // sensitivity gained per orgasm
  climaxAt:10,           // sec before CLIMAX unlocks
  spurtNeed:7
};
/* rub zones: 0 both breasts · 1 left · 2 right · 3 low — full user control via C/Q/1-4 */
const RUBZONES=['both','left','right','low'];
const RUBLBL={both:'BOTH',left:'LEFT',right:'RIGHT',low:'LOW'};
const RUBGAIN={both:1,left:.85,right:.85,low:1.4};
const CV=document.getElementById('scene'), X=CV.getContext('2d');
const stage=document.getElementById('stage');
const W=1280,H=720; let SC=1;

const clamp=(v,a,b)=>v<a?a:v>b?b:v;
const lerp=(a,b,t)=>a+(b-a)*t;
const sm=(a,b,v)=>{v=clamp((v-a)/(b-a),0,1);return v*v*(3-2*v)};
const R=Math.random, rr=(a,b)=>a+R()*(b-a);
const TAU=Math.PI*2;

/* ---------------- game state ---------------- */
const G={
  state:'intro', t:0, sesT:0,
  target:.28, depth:.28, pDepth:.28, vel:0, prevV:0, maxD:0, valley:.28,
  lastStrokeT:-9, strokeDur:0, strokes:0, inBand:0,
  rate:0, combo:1, comboBest:1,
  pleasure:12, floor:2, sens:1,
  stamina:100, tired:false, tiredT:0,
  kiss:0, rub:0, kissT:0, rubT:0, rubZone:0,
  oral:0, oralT:0, oralDepth:0, oralGag:0, pos:0,
  solo:false, soloPh:0, soloMoanT:3,
  jets:[], shaftPulse:0,
  orgasms:0, orgT:0, after:0,
  climax:false, spurts:0, climaxT:0, finishT:0, sync:false, endedShown:false,
  ar:0, round:1,
  hotC:.55, shake:0, bloom:0, impact:0,
  breast:{p:0,v:0}, butt:{p:0,v:0},
  blink:2, blinkPh:-1,
  mouths:[], nextMoan:2, nextBreath:1, nextBeat:0,
  sayCd:0, speech:null,
  sweat:[], hearts:[], drips:[], glisten:[],
  strokeFlash:0, hotFlash:0, dragOn:false, nod:0,
  spaceHeld:false, autoPh:-Math.PI/2,
  view:'side', viewFade:0,
  faceSide:'profile', faceBlend:0,
  fpvFocus:'full', fpvZoom:1, fpvPanX:640, fpvPanY:360,
  char:{
    preset:'yuki', name:'Yuki',
    quote:'“Mm… finally we’re alone. You remember how I like it… right?”',
    skinTone:.18, hairColor:'#231318', hairStyle:'long',
    bodyScale:.45, breastSize:.45,
    nippleColor:'#c25f63', blushColor:'#e86070', lipColor:'#b3555f', eyeColor:'#4a2c33',
    pubicHair:'trim'
  }
};
