// Afterglow — module: charui (loaded by index.html)
'use strict';
/* ============================================================
   CHARACTER SELECTOR OVERLAY + CUSTOMIZATION PANEL
   #charSel  → pre-game character picker (3 portrait cards)
   #custPanel → in-game live customization drawer (P key)
   ============================================================ */

/* ---------- character selector ---------- */
function buildCharSelector(){
  const el=document.createElement('div');
  el.id='charSel'; el.className='ov';
  el.innerHTML=`
<div class="charCard">
  <div class="chip18">18+</div>
  <h1>Afterglow</h1>
  <div class="sub">choose your companion — tap her to begin</div>
  <div class="adultNote"><b>18+</b> adults only · every character is a consenting adult</div>
  <div id="charRow"></div>
  <div id="charQuote" class="quote"></div>
  <div class="charBtnRow">
    <button class="linkbtn" id="btnCharCustom">✦ customize before starting</button>
  </div>
</div>`;
  document.getElementById('stage').prepend(el);

  // build portrait cards — tapping one selects AND starts the night directly
  const row=el.querySelector('#charRow');
  Object.values(CHARS).forEach(c=>{
    const card=document.createElement('div');
    card.className='cCard'; card.dataset.preset=c.preset; card.tabIndex=0;
    card.setAttribute('role','button'); card.setAttribute('aria-label','Choose '+c.name);
    card.innerHTML=`<canvas class="cPortrait" width="280" height="360"></canvas>
      <div class="cName">${c.name}</div>`;
    const go=()=>{
      if(card.classList.contains('picked')) return;
      selectChar(c.preset);
      if(G.state!=='intro') return;
      card.classList.add('picked');
      if(typeof ai==='function') ai();          // unlock audio inside the gesture
      setTimeout(()=>{ card.classList.remove('picked'); startNight(); },420);
    };
    card.addEventListener('click',go);
    card.addEventListener('keydown',e=>{ if(e.key==='Enter'||e.key===' '){ e.preventDefault(); go(); } });
    // hover: preview her line and tilt the card toward the pointer
    card.addEventListener('mouseenter',()=>{ const q=document.getElementById('charQuote'); if(q) q.textContent=c.quote; });
    card.addEventListener('mousemove',e=>{
      const r=card.getBoundingClientRect(), x=(e.clientX-r.left)/r.width, y=(e.clientY-r.top)/r.height;
      card.style.setProperty('--ry',((x-.5)*14).toFixed(1)+'deg');
      card.style.setProperty('--rx',((.5-y)*12).toFixed(1)+'deg');
      card.style.setProperty('--mx',(x*100).toFixed(0)+'%');
      card.style.setProperty('--my',(y*100).toFixed(0)+'%');
    });
    card.addEventListener('mouseleave',()=>{
      card.style.setProperty('--rx','0deg'); card.style.setProperty('--ry','0deg');
      const q=document.getElementById('charQuote'); if(q&&CHARS[G.char.preset]) q.textContent=CHARS[G.char.preset].quote;
    });
    row.appendChild(card);
    setTimeout(()=>renderPortrait(card.querySelector('.cPortrait'),c),0);
  });

  document.getElementById('btnCharCustom').onclick=()=>{
    if(G.state==='intro') startNight();
    setTimeout(()=>openCustomPanel(),200);
  };

  // restore selection
  selectChar(G.char.preset,true);
}

/* shared start: skip the intro screen, go straight to play.
   Same unlock + state flip the old CONTINUE → TOUCH HER chain did. */
function startNight(){
  if(typeof ai==='function') ai();
  try{ if(typeof AC!=='undefined'&&AC&&AC.resume) AC.resume(); }catch(_){}
  const cs=document.getElementById('charSel'); if(cs) cs.classList.add('hide');
  const intro=document.getElementById('intro'); if(intro) intro.classList.add('hide');
  G.state='play'; G.sesT=0;
}

function selectChar(key, silent){
  applyPreset(key);
  document.querySelectorAll('.cCard').forEach(c=>{
    c.classList.toggle('selected', c.dataset.preset===key);
  });
  const q=document.getElementById('charQuote');
  if(q) q.textContent=CHARS[key]?CHARS[key].quote:'';
  if(!silent && typeof updatePlName==='function') updatePlName();
}

/* ---------- portrait renderer ----------
   A lamp-lit head-and-shoulders study of an adult woman: defined jaw and
   cheekbones, long neck, collarbones; hair framing per preset. */
function renderPortrait(cv,c){
  const cx=cv.getContext('2d');
  const W=140,H=180, TAU2=Math.PI*2;
  cx.setTransform(cv.width/W,0,0,cv.height/H,0,0);
  const E=(x,y,rx,ry,r=0)=>{ cx.beginPath(); cx.ellipse(x,y,rx,ry,r,0,TAU2); };
  const rg=(x0,y0,r0,x1,y1,r1,st)=>{ const g=cx.createRadialGradient(x0,y0,r0,x1,y1,r1); st.forEach(s=>g.addColorStop(s[0],s[1])); return g; };
  const lg=(x0,y0,x1,y1,st)=>{ const g=cx.createLinearGradient(x0,y0,x1,y1); st.forEach(s=>g.addColorStop(s[0],s[1])); return g; };
  const hc=c.hairColor, hcL=lerpHex(hc,'#ffe2d0',.28), hcD=lerpHex(hc,'#000000',.45);
  const t=c.skinTone;
  const sk=lerpHex3('#f6d0b0','#d4936a','#7a4028',t), skL=lerpHex(sk,'#fff2e6',.35), skS=lerpHex(sk,'#3a1410',.28), skD=lerpHex(sk,'#2a0c0a',.5);
  // backdrop: warm room bokeh
  cx.fillStyle=lg(0,0,0,H,[[0,'#2a1220'],[1,'#0e0609']]); cx.fillRect(0,0,W,H);
  [[22,30,14,'255,170,110',.22],[118,24,10,'255,120,150',.18],[110,70,16,'255,190,130',.12],[16,96,9,'255,140,160',.14],[128,120,12,'200,170,255',.08]]
    .forEach(([x,y,r,col,a])=>{ cx.fillStyle=rg(x,y,0,x,y,r,[[0,`rgba(${col},${a})`],[1,`rgba(${col},0)`]]); E(x,y,r,r); cx.fill(); });
  cx.fillStyle=rg(36,60,4,50,80,110,[[0,'rgba(255,180,120,.28)'],[1,'rgba(255,180,120,0)']]); cx.fillRect(0,0,W,H);
  // hair: back mass (style dependent)
  cx.fillStyle=lg(0,30,0,H,[[0,hcL],[.4,hc],[1,hcD]]);
  if(c.hairStyle==='natural'){
    E(70,70,52,50); cx.fill();
    for(let i=0;i<22;i++){ const a=i/22*TAU2, r=46+(i%3)*4; E(70+Math.cos(a)*r,70+Math.sin(a)*r*.95,9,9); cx.fill(); }
  } else {
    cx.beginPath(); cx.moveTo(30,64); cx.bezierCurveTo(26,20,114,20,110,64);
    cx.bezierCurveTo(118,110,122,150,112,180); cx.lineTo(28,180); cx.bezierCurveTo(18,150,22,110,30,64); cx.fill();
  }
  // shoulders + chest (bare), lit from the upper-left
  cx.fillStyle=lg(10,140,130,170,[[0,skL],[.45,sk],[1,skS]]);
  cx.beginPath(); cx.moveTo(4,H); cx.bezierCurveTo(8,150,34,140,56,136); cx.lineTo(84,136);
  cx.bezierCurveTo(106,140,132,150,136,H); cx.closePath(); cx.fill();
  // neck: long, with a soft shadow under the jaw
  cx.fillStyle=lg(56,0,86,0,[[0,skL],[.5,sk],[1,skS]]);
  cx.beginPath(); cx.moveTo(57,104); cx.lineTo(56,140); cx.quadraticCurveTo(70,146,84,140); cx.lineTo(83,104); cx.closePath(); cx.fill();
  cx.fillStyle=rg(70,110,2,70,112,18,[[0,'rgba(60,20,16,.35)'],[1,'rgba(60,20,16,0)']]); E(70,112,18,10); cx.fill();
  // collarbones + sternal notch
  cx.strokeStyle=`rgba(${hexToRgb(skD).join(',')},.35)`; cx.lineWidth=1.2; cx.lineCap='round';
  cx.beginPath(); cx.moveTo(34,150); cx.quadraticCurveTo(50,144,64,147); cx.moveTo(76,147); cx.quadraticCurveTo(90,144,106,150); cx.stroke();
  cx.fillStyle=`rgba(${hexToRgb(skL).join(',')},.35)`; E(46,146,10,2,-.15); cx.fill();
  // horns / ears behind the face
  if(c.hairStyle==='horns'||c.preset==='goatchan'){
    cx.fillStyle=lg(0,10,0,60,[[0,'#fffaf4'],[1,'#c9b8a8']]);
    cx.beginPath(); cx.moveTo(46,50); cx.bezierCurveTo(30,40,20,24,26,8); cx.bezierCurveTo(34,24,46,32,56,40); cx.fill();
    cx.beginPath(); cx.moveTo(94,50); cx.bezierCurveTo(110,40,120,24,114,8); cx.bezierCurveTo(106,24,94,32,84,40); cx.fill();
    cx.strokeStyle='rgba(120,90,70,.35)'; cx.lineWidth=.8;
    for(let i=0;i<4;i++){ cx.beginPath(); cx.moveTo(30+i*4,30-i*5); cx.lineTo(36+i*4,34-i*5); cx.moveTo(110-i*4,30-i*5); cx.lineTo(104-i*4,34-i*5); cx.stroke(); }
  }
  if(c.preset==='kiyoko'){
    [[-1,44],[1,96]].forEach(([s,x])=>{
      cx.fillStyle=hc; cx.beginPath(); cx.moveTo(x,50); cx.lineTo(x-s*8,8); cx.lineTo(x+s*20,38); cx.closePath(); cx.fill();
      cx.fillStyle='rgba(250,170,160,.85)'; cx.beginPath(); cx.moveTo(x+s*2,46); cx.lineTo(x-s*4,18); cx.lineTo(x+s*14,38); cx.closePath(); cx.fill();
    });
  }
  // face: adult oval — tapered jaw, cheekbones, defined chin
  const face=()=>{ cx.beginPath(); cx.moveTo(70,36); cx.bezierCurveTo(94,36,101,56,99,76);
    cx.bezierCurveTo(97,94,86,110,70,116); cx.bezierCurveTo(54,110,43,94,41,76); cx.bezierCurveTo(39,56,46,36,70,36); cx.closePath(); };
  face(); cx.fillStyle=rg(60,62,4,70,76,46,[[0,skL],[.5,sk],[.85,skS],[1,skD]]); cx.fill();
  cx.save(); face(); cx.clip();
  cx.fillStyle=rg(94,86,2,94,86,20,[[0,'rgba(60,20,16,.28)'],[1,'rgba(60,20,16,0)']]); cx.fillRect(0,0,W,H);   // far-cheek turn
  cx.fillStyle=rg(70,114,2,70,114,16,[[0,'rgba(60,20,16,.25)'],[1,'rgba(60,20,16,0)']]); cx.fillRect(0,0,W,H);  // chin underside
  const bl=hexToRgb(c.blushColor).join(',');
  cx.fillStyle=rg(54,86,0,54,86,12,[[0,`rgba(${bl},.38)`],[1,`rgba(${bl},0)`]]); E(54,86,12,8); cx.fill();
  cx.fillStyle=rg(87,86,0,87,86,11,[[0,`rgba(${bl},.32)`],[1,`rgba(${bl},0)`]]); E(87,86,11,7); cx.fill();
  cx.fillStyle='rgba(255,246,236,.28)'; E(56,66,7,3,-.3); cx.fill(); E(68,48,10,4); cx.fill();                  // cheekbone + forehead light
  cx.restore();
  // eyes: almond, heavy upper lash, iris gradient, catchlight; brows above
  const eyeC=c.eyeColor;
  [[57,-1],[84,1]].forEach(([x,s])=>{
    const y=77;
    cx.fillStyle='rgba(60,25,20,.18)'; E(x,y-2,10,5); cx.fill();                             // socket shade
    cx.fillStyle='#f7ece8'; cx.beginPath(); cx.moveTo(x-8,y); cx.quadraticCurveTo(x,y-6.5,x+8,y-.5); cx.quadraticCurveTo(x,y+4.5,x-8,y); cx.fill();
    cx.save(); cx.clip();
    cx.fillStyle=rg(x,y-1,.5,x,y-.5,4.6,[[0,lerpHex(eyeC,'#ffffff',.35)],[.6,eyeC],[1,'#160609']]); E(x+s*.4,y-.6,4.4,4.6); cx.fill();
    cx.fillStyle='#120406'; E(x+s*.4,y-.6,1.9,2.1); cx.fill();
    cx.restore();
    cx.fillStyle='rgba(255,255,255,.9)'; E(x-1.4+s*.4,y-2.4,1.2,1); cx.fill();
    cx.strokeStyle='#1c0c10'; cx.lineWidth=1.9; cx.lineCap='round';
    cx.beginPath(); cx.moveTo(x-8.6,y+.2); cx.quadraticCurveTo(x,y-7.6,x+8.6,y-1.2); cx.lineTo(x+10.4*1,y-3.2+(s>0?0:0)); cx.stroke();
    cx.strokeStyle='rgba(40,16,18,.4)'; cx.lineWidth=.8;
    cx.beginPath(); cx.moveTo(x-6,y+2.6); cx.quadraticCurveTo(x,y+4.6,x+7,y+1.6); cx.stroke();
    cx.strokeStyle=hcD; cx.lineWidth=2.2;
    cx.beginPath(); cx.moveTo(x-9,y-10+(s<0?1:0)); cx.quadraticCurveTo(x-1,y-14,x+9,y-10.5); cx.stroke();
  });
  // nose: bridge light + soft tip shadow
  cx.strokeStyle='rgba(255,240,226,.35)'; cx.lineWidth=1.2; cx.beginPath(); cx.moveTo(69,80); cx.lineTo(68,92); cx.stroke();
  cx.fillStyle='rgba(90,40,30,.25)'; E(72,96,4.5,2); cx.fill();
  cx.strokeStyle='rgba(90,40,30,.45)'; cx.lineWidth=.9; cx.beginPath(); cx.moveTo(66,96.5); cx.quadraticCurveTo(70,98.5,74,96.5); cx.stroke();
  // lips: full, glossy, slightly parted
  const lip=c.lipColor;
  cx.fillStyle=lg(0,100,0,110,[[0,lerpHex(lip,'#ffffff',.12)],[1,lerpHex(lip,'#000000',.2)]]);
  cx.beginPath(); cx.moveTo(60,104); cx.quadraticCurveTo(65,100.5,70,102); cx.quadraticCurveTo(75,100.5,80,104); cx.quadraticCurveTo(70,106,60,104); cx.fill();
  cx.beginPath(); cx.moveTo(61,104.4); cx.quadraticCurveTo(70,112.5,79,104.4); cx.quadraticCurveTo(70,106.4,61,104.4); cx.fill();
  cx.strokeStyle='rgba(70,16,24,.7)'; cx.lineWidth=.9; cx.beginPath(); cx.moveTo(60.5,104.2); cx.quadraticCurveTo(70,106.6,79.5,104.2); cx.stroke();
  cx.fillStyle='rgba(255,240,240,.55)'; E(72,108,3.4,1.1); cx.fill();
  // front hair: side-swept fringe + locks over the shoulders
  cx.fillStyle=lg(0,30,0,90,[[0,hcL],[.5,hc],[1,hcD]]);
  if(c.hairStyle==='natural'){
    cx.beginPath(); cx.moveTo(38,70); cx.bezierCurveTo(34,30,106,26,102,70); cx.bezierCurveTo(96,48,80,42,70,44); cx.bezierCurveTo(58,42,44,48,38,70); cx.fill();
  } else {
    cx.beginPath(); cx.moveTo(36,78); cx.bezierCurveTo(30,34,104,24,104,66);
    cx.bezierCurveTo(96,50,86,44,74,48); cx.bezierCurveTo(66,56,54,60,46,74); cx.bezierCurveTo(44,76,40,80,36,78); cx.fill();
    cx.beginPath(); cx.moveTo(40,72); cx.bezierCurveTo(30,100,34,130,26,158); cx.quadraticCurveTo(38,140,44,118); cx.bezierCurveTo(46,100,44,86,46,74); cx.fill();
    cx.beginPath(); cx.moveTo(100,66); cx.bezierCurveTo(108,100,104,130,114,158); cx.quadraticCurveTo(100,140,96,118); cx.bezierCurveTo(94,100,96,84,96,70); cx.fill();
  }
  cx.strokeStyle='rgba(255,230,210,.22)'; cx.lineWidth=1.3;
  cx.beginPath(); cx.moveTo(46,46); cx.quadraticCurveTo(62,34,84,38); cx.moveTo(42,56); cx.quadraticCurveTo(52,44,66,42); cx.stroke();
  // vignette + warm rim on the lamp side
  cx.fillStyle=rg(70,90,40,70,90,120,[[0,'rgba(0,0,0,0)'],[1,'rgba(0,0,0,.55)']]); cx.fillRect(0,0,W,H);
}

/* ---------- customization panel ---------- */
let custOpen=false;
function buildCustomPanel(){
  const el=document.createElement('div');
  el.id='custPanel';
  el.innerHTML=`<div class="cpHead"><span>\u2726 Customize</span><button id="cpClose">\u2715</button></div>
<div class="cpBody">
  <div class="cpSection">
    <div class="cpLabel">CHARACTER</div>
    <div class="cpPresets">${Object.values(CHARS).map(c=>`<button class="cpPre" data-p="${c.preset}">${c.name}</button>`).join('')}</div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">POSITION / ACT</div>
    <div class="cpToggle" id="cpPosTog" style="display:grid;grid-template-columns:repeat(4,1fr);gap:4px">
      <button class="cpTog" data-pos="0">Missionary</button>
      <button class="cpTog" data-pos="1">Legs-Up</button>
      <button class="cpTog" data-pos="2">Doggy</button>
      <button class="cpTog" data-pos="3">Prone</button>
      <button class="cpTog" data-pos="4">Cowgirl</button>
      <button class="cpTog" data-pos="5">Rev Cowgirl</button>
      <button class="cpTog" data-pos="6">Spoon</button>
      <button class="cpTog" data-pos="oral" style="color:#ff85a0">Oral</button>
    </div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">NAME</div>
    <input class="cpInput" id="cpName" type="text" maxlength="18" placeholder="Her name">
  </div>
  <div class="cpSection">
    <div class="cpLabel">SKIN TONE</div>
    <div class="cpSkinTrack"><input class="cpSlider" id="cpSkin" type="range" min="0" max="100" step="1"></div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">HAIR COLOR</div>
    <div class="cpSwatches" id="cpHairSwatches">
      ${['#231318','#1a0e0a','#3d1e10','#6b3c0e','#c9a060'].map(h=>`<button class="cpSw" data-col="${h}" style="background:${h}"></button>`).join('')}
      <input class="cpColor" type="color" id="cpHairPick" title="custom">
    </div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">HAIR STYLE</div>
    <div class="cpToggle">
      <button class="cpTog" data-hs="long">Long</button>
      <button class="cpTog" data-hs="bob">Bob</button>
      <button class="cpTog" data-hs="natural">Natural</button>
    </div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">FPV CLOSEUP / ZOOM</div>
    <div class="cpToggle" id="cpFpvTog">
      <button class="cpTog" data-fpv="full">Full</button>
      <button class="cpTog" data-fpv="breasts">Breasts</button>
      <button class="cpTog" data-fpv="hips">Spot</button>
      <button class="cpTog" data-fpv="face">Face</button>
    </div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">BODY SCALE</div>
    <input class="cpSlider" id="cpBody" type="range" min="0" max="100" step="1">
  </div>
  <div class="cpSection">
    <div class="cpLabel">BREAST SIZE</div>
    <input class="cpSlider" id="cpBreast" type="range" min="0" max="100" step="1">
  </div>
  <div class="cpSection">
    <div class="cpLabel">PUBIC HAIR</div>
    <div class="cpToggle">
      <button class="cpTog" data-ph="bare">Bare</button>
      <button class="cpTog" data-ph="trim">Trim</button>
      <button class="cpTog" data-ph="full">Full</button>
    </div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">LIP COLOR</div>
    <div class="cpSwatches" id="cpLipSwatches">
      ${['#b3555f','#943040','#6a2435','#d07070','#c08070'].map(h=>`<button class="cpSw" data-lc="${h}" style="background:${h}"></button>`).join('')}
      <input class="cpColor" type="color" id="cpLipPick" title="custom">
    </div>
  </div>
  <div class="cpSection">
    <div class="cpLabel">NIPPLE COLOR</div>
    <div class="cpSwatches" id="cpNipSwatches">
      ${['#c25f63','#a04848','#6a2828','#e08080','#b07060'].map(h=>`<button class="cpSw" data-nc="${h}" style="background:${h}"></button>`).join('')}
      <input class="cpColor" type="color" id="cpNipPick" title="custom">
    </div>
  </div>
  <div class="cpSection" id="cpGlbSection" style="border-top:1px solid rgba(255,255,255,0.14);padding-top:10px;margin-top:10px;">
    <div class="cpLabel" style="color:#ff6987">\u2726 GLB ATTIRE &amp; BODY PARTS</div>
    <div class="cpToggle" id="cpGlbToggles" style="display:grid;grid-template-columns:repeat(2,1fr);gap:4px">
      <button class="cpTog on" data-glbpart="tail">Tail: ON</button>
      <button class="cpTog on" data-glbpart="horns">Horns: ON</button>
      <button class="cpTog on" data-glbpart="ears">Ears: ON</button>
      <button class="cpTog on" data-glbpart="socks">Socks: ON</button>
      <button class="cpTog on" data-glbpart="accessories">Harness: ON</button>
      <button class="cpTog on" data-glbpart="nippless">Pasties: ON</button>
      <button class="cpTog on" data-glbpart="runes">Runes: ON</button>
      <button class="cpTog on" data-glbpart="genital">Genitals: ON</button>
    </div>
    <div style="margin-top:8px;font-size:10px;color:#aaa;letter-spacing:.05em">BODY RUNES GLOW COLOR</div>
    <div style="display:flex;align-items:center;gap:8px;margin-top:4px">
      <input class="cpColor" type="color" id="cpRunePick" value="#ff3366" title="rune color">
      <span style="font-size:11px;color:#888">Marking tint</span>
    </div>
  </div>
</div>`;
  document.getElementById('stage').appendChild(el);
  // prevent stage pointerdown dragging when interacting with sliders/inputs
  ['pointerdown','mousedown','touchstart'].forEach(evt=>{
    el.addEventListener(evt,e=>e.stopPropagation());
  });
  el.querySelector('#cpClose').onclick=closeCustomPanel;

  // preset buttons
  el.querySelectorAll('.cpPre').forEach(b=>b.addEventListener('click',()=>{
    applyPreset(b.dataset.p); syncPanelFromChar();
  }));
  // name
  el.querySelector('#cpName').addEventListener('input',e=>{
    G.char.name=e.target.value||'Her'; lsSaveChar(); if(typeof updatePlName==='function')updatePlName();
  });
  // skin slider
  el.querySelector('#cpSkin').addEventListener('input',e=>{
    G.char.skinTone=e.target.value/100; lsSaveChar();
    if(typeof glbApplyCustom==='function') glbApplyCustom(G.char.preset);
  });
  // hair swatches
  el.querySelectorAll('[data-col]').forEach(b=>b.addEventListener('click',()=>{
    G.char.hairColor=b.dataset.col; lsSaveChar(); syncSwatches();
    if(typeof glbSetPartColor==='function') glbSetPartColor(G.char.preset, 'hair', G.char.hairColor);
  }));
  el.querySelector('#cpHairPick').addEventListener('input',e=>{
    G.char.hairColor=e.target.value; lsSaveChar(); syncSwatches();
    if(typeof glbSetPartColor==='function') glbSetPartColor(G.char.preset, 'hair', G.char.hairColor);
  });
  // hair style
  el.querySelectorAll('[data-hs]').forEach(b=>b.addEventListener('click',()=>{
    G.char.hairStyle=b.dataset.hs; lsSaveChar(); syncToggles();
  }));
  // body / breast
  el.querySelector('#cpBody').addEventListener('input',e=>{
    G.char.bodyScale=e.target.value/100; lsSaveChar();
    if(typeof glbApplyCustom==='function') glbApplyCustom(G.char.preset);
  });
  el.querySelector('#cpBreast').addEventListener('input',e=>{
    G.char.breastSize=e.target.value/100; lsSaveChar();
    if(typeof glbSetScale==='function') glbSetScale('breast', 0.65 + G.char.breastSize * 0.9);
  });
  // GLB parts toggles
  el.querySelectorAll('[data-glbpart]').forEach(b=>b.addEventListener('click',()=>{
    const pk = b.dataset.glbpart;
    const curVis = !b.classList.contains('off');
    const newVis = !curVis;
    b.classList.toggle('off', !newVis);
    b.classList.toggle('on', newVis);
    const label = pk.charAt(0).toUpperCase() + pk.slice(1);
    b.textContent = label + ': ' + (newVis ? 'ON' : 'OFF');
    if(typeof glbSetPartVisible==='function') glbSetPartVisible(G.char.preset, pk, newVis);
  }));
  // GLB rune color picker
  const rp = el.querySelector('#cpRunePick');
  if (rp) {
    rp.addEventListener('input', e=>{
      if(typeof glbSetPartColor==='function') glbSetPartColor(G.char.preset, 'runes', e.target.value);
    });
  }
  // fpv closeup toggles
  el.querySelectorAll('[data-fpv]').forEach(b=>b.addEventListener('click',()=>{
    G.fpvFocus=b.dataset.fpv;
    if(G.view!=='fpv'){ toggleView(); }
    const bz=document.getElementById('btnZoom');
    if(bz) bz.classList.toggle('on', G.fpvFocus!=='full');
    syncToggles();
  }));
  // pubic
  el.querySelectorAll('[data-ph]').forEach(b=>b.addEventListener('click',()=>{
    G.char.pubicHair=b.dataset.ph; lsSaveChar(); syncToggles();
  }));
  // lip swatches
  el.querySelectorAll('[data-lc]').forEach(b=>b.addEventListener('click',()=>{
    G.char.lipColor=b.dataset.lc; lsSaveChar(); syncSwatches();
  }));
  el.querySelector('#cpLipPick').addEventListener('input',e=>{ G.char.lipColor=e.target.value; lsSaveChar(); syncSwatches(); });
  // nipple swatches
  el.querySelectorAll('[data-nc]').forEach(b=>b.addEventListener('click',()=>{
    G.char.nippleColor=b.dataset.nc; lsSaveChar(); syncSwatches();
  }));
  el.querySelector('#cpNipPick').addEventListener('input',e=>{ G.char.nippleColor=e.target.value; lsSaveChar(); syncSwatches(); });

  // position / act buttons
  el.querySelectorAll('[data-pos]').forEach(b=>b.addEventListener('click',()=>{
    const p=b.dataset.pos;
    if(p==='oral'){
      if(typeof toggleOral==='function') toggleOral();
    } else {
      G.oralT=0; G.pos=parseInt(p,10)||0; G.nod=1;
      if(typeof POSES!=='undefined'&&POSES[G.pos]) say(POSES[G.pos].line,1.8);
      if(typeof playMoan==='function') playMoan(.38,{dur:.4,pmul:1.15,vol:.85});
    }
    syncToggles();
  }));

  syncPanelFromChar();
}
function syncPanelFromChar(){
  const p=document.getElementById('custPanel'); if(!p) return;
  p.querySelector('#cpName').value=G.char.name;
  p.querySelector('#cpSkin').value=Math.round(G.char.skinTone*100);
  p.querySelector('#cpBody').value=Math.round(G.char.bodyScale*100);
  p.querySelector('#cpBreast').value=Math.round(G.char.breastSize*100);
  p.querySelector('#cpHairPick').value=G.char.hairColor;
  p.querySelector('#cpLipPick').value=G.char.lipColor;
  p.querySelector('#cpNipPick').value=G.char.nippleColor;
  const glbSec = p.querySelector('#cpGlbSection');
  const isGLB = G.char && (G.char.isGLB || G.char.preset === 'goatchan');
  if (glbSec) glbSec.style.display = isGLB ? 'block' : 'none';

  if (isGLB) {
    const custom = (G.char && G.char.glbCustom) || {};
    const parts = custom.parts || {};
    p.querySelectorAll('[data-glbpart]').forEach(b => {
      const pk = b.dataset.glbpart;
      const vis = parts[pk] !== undefined ? parts[pk] : true;
      b.classList.toggle('off', !vis);
      b.classList.toggle('on', vis);
      const label = pk.charAt(0).toUpperCase() + pk.slice(1);
      b.textContent = label + ': ' + (vis ? 'ON' : 'OFF');
    });
    const rp = p.querySelector('#cpRunePick');
    if (rp && custom.colors && custom.colors.runes) {
      rp.value = custom.colors.runes;
    }
  }
  syncSwatches(); syncToggles();
  p.querySelectorAll('.cpPre').forEach(b=>b.classList.toggle('on',b.dataset.p===G.char.preset));
}
function syncSwatches(){
  const p=document.getElementById('custPanel'); if(!p) return;
  p.querySelectorAll('[data-col]').forEach(b=>b.classList.toggle('on',b.dataset.col===G.char.hairColor));
  p.querySelectorAll('[data-lc]').forEach(b=>b.classList.toggle('on',b.dataset.lc===G.char.lipColor));
  p.querySelectorAll('[data-nc]').forEach(b=>b.classList.toggle('on',b.dataset.nc===G.char.nippleColor));
}
function syncToggles(){
  const p=document.getElementById('custPanel'); if(!p) return;
  p.querySelectorAll('[data-hs]').forEach(b=>b.classList.toggle('on',b.dataset.hs===G.char.hairStyle));
  p.querySelectorAll('[data-ph]').forEach(b=>b.classList.toggle('on',b.dataset.ph===G.char.pubicHair));
  p.querySelectorAll('[data-fpv]').forEach(b=>b.classList.toggle('on',b.dataset.fpv===(G.fpvFocus||'full')));
  p.querySelectorAll('[data-pos]').forEach(b=>{
    const posVal=b.dataset.pos;
    if(posVal==='oral') b.classList.toggle('on',G.oralT>0);
    else b.classList.toggle('on',G.oralT===0 && (G.pos|0)===parseInt(posVal,10));
  });
}
function openCustomPanel(){ custOpen=true; document.getElementById('custPanel').classList.add('open'); syncPanelFromChar(); }
function closeCustomPanel(){ custOpen=false; document.getElementById('custPanel').classList.remove('open'); }
function toggleCustomPanel(){ custOpen?closeCustomPanel():openCustomPanel(); }

/* ---------- DOM helpers in app.js ---------- */
function updatePlName(){
  const el=document.getElementById('plName'); if(!el) return;
  const name=(G.char&&G.char.name)||'Yuki';
  el.innerHTML=name+` <svg viewBox="0 0 24 24"><path d="M12 20.3S3.6 15 1.9 9.9C.7 6.3 3.2 3 6.6 3c2.2 0 3.9 1.3 4.7 2.7h1.4C13.5 4.3 15.2 3 17.4 3c3.4 0 5.9 3.3 4.7 6.9C20.4 15 12 20.3 12 20.3z"/></svg>`;
  const introSub=document.querySelector('#intro .sub');
  if(introSub) introSub.textContent=`a slow night in with ${name}`;
  const introQuote=document.querySelector('#intro .quote');
  if(introQuote && G.char && G.char.quote) introQuote.innerHTML=`${G.char.quote}<br>Don't just stare — <b>touch me</b>.`;
  const btnB=document.getElementById('btnBegin');
  if(btnB) btnB.textContent=`TOUCH ${name.toUpperCase()}`;
}

/* ---------- init (called from app.js after DOM ready) ---------- */
function initCharUI(){
  buildCharSelector();
  buildCustomPanel();
}
