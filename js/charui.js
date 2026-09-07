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
  <div class="sub">choose your companion</div>
  <div id="charRow"></div>
  <div id="charQuote" class="quote"></div>
  <div style="display:flex;gap:1em;justify-content:center;flex-wrap:wrap;margin-top:2em">
    <button class="bigbtn" id="btnCharBegin">CONTINUE \u2192</button>
    <button class="bigbtn ghost" id="btnCharCustom">\u2726 CUSTOMIZE</button>
  </div>
</div>`;
  document.getElementById('stage').prepend(el);

  // build portrait cards
  const row=el.querySelector('#charRow');
  Object.values(CHARS).forEach(c=>{
    const card=document.createElement('div');
    card.className='cCard'; card.dataset.preset=c.preset;
    card.innerHTML=`<canvas class="cPortrait" width="140" height="180"></canvas>
      <div class="cName">${c.name}</div>`;
    card.addEventListener('click',()=>selectChar(c.preset));
    row.appendChild(card);
    // draw mini portrait
    setTimeout(()=>renderPortrait(card.querySelector('.cPortrait'),c),0);
  });

  document.getElementById('btnCharBegin').onclick=()=>{
    document.getElementById('charSel').classList.add('hide');
    document.getElementById('intro').classList.remove('hide');
  };
  document.getElementById('btnCharCustom').onclick=()=>{
    document.getElementById('charSel').classList.add('hide');
    document.getElementById('intro').classList.remove('hide');
    setTimeout(()=>openCustomPanel(),200);
  };

  // restore selection
  selectChar(G.char.preset,true);
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

/* ---------- mini portrait renderer ---------- */
function renderPortrait(cv,c){
  const cx=cv.getContext('2d');
  const W=140,H=180;
  cx.fillStyle='#1a0d12'; cx.fillRect(0,0,W,H);
  // mood gradient bg
  const bg=cx.createLinearGradient(0,0,0,H);
  bg.addColorStop(0,'#231318'); bg.addColorStop(1,'#120a0d');
  cx.fillStyle=bg; cx.fillRect(0,0,W,H);
  // skin
  const t=c.skinTone;
  const skinC=lerpHex3('#f4cba8','#d4936a','#7a4028',t);
  const skinSh=lerpHex3('#cf9273','#a96840','#582815',t);
  // hair spill
  cx.fillStyle=c.hairColor;
  cx.beginPath(); cx.ellipse(70,80,42,50,0,0,Math.PI*2); cx.fill();
  // face oval
  cx.fillStyle=skinC;
  cx.beginPath(); cx.ellipse(70,78,30,36,0,0,Math.PI*2); cx.fill();
  // jaw
  cx.beginPath(); cx.moveTo(44,82); cx.quadraticCurveTo(40,112,70,122);
  cx.quadraticCurveTo(100,112,96,82); cx.fill();
  // neck + shoulders (bare suggestion)
  cx.fillStyle=skinSh;
  cx.beginPath(); cx.ellipse(70,132,22,12,0,0,Math.PI*2); cx.fill();
  cx.fillStyle=skinC;
  cx.beginPath(); cx.ellipse(70,128,14,18,0,0,Math.PI*2); cx.fill();
  // blush
  cx.fillStyle=`rgba(${hexToRgb(c.blushColor).join(',')},0.35)`;
  cx.beginPath(); cx.ellipse(52,84,13,8,-.2,0,Math.PI*2); cx.fill();
  cx.beginPath(); cx.ellipse(88,84,13,8,.2,0,Math.PI*2); cx.fill();
  // eyes (simple)
  cx.fillStyle='#f6e9e4';
  cx.beginPath(); cx.ellipse(58,74,8,4.5,0,0,Math.PI*2); cx.fill();
  cx.beginPath(); cx.ellipse(82,74,8,4.5,0,0,Math.PI*2); cx.fill();
  cx.fillStyle=c.eyeColor;
  cx.beginPath(); cx.arc(59,74,3,0,Math.PI*2); cx.fill();
  cx.beginPath(); cx.arc(83,74,3,0,Math.PI*2); cx.fill();
  cx.fillStyle='rgba(255,255,255,.8)';
  cx.beginPath(); cx.arc(58,73,1,0,Math.PI*2); cx.fill();
  cx.beginPath(); cx.arc(82,73,1,0,Math.PI*2); cx.fill();
  // lashes
  cx.strokeStyle='#2a181d'; cx.lineWidth=1.6;
  cx.beginPath(); cx.moveTo(50,70); cx.quadraticCurveTo(58,66,66,70); cx.stroke();
  cx.beginPath(); cx.moveTo(74,70); cx.quadraticCurveTo(82,66,90,70); cx.stroke();
  // lips
  cx.fillStyle=c.lipColor;
  cx.beginPath(); cx.ellipse(70,100,8,3.5,0,0,Math.PI*2); cx.fill();
  // name label bg
  cx.fillStyle='rgba(18,9,13,.65)';
  cx.fillRect(0,150,W,30);
  cx.fillStyle='#ffe9ef';
  cx.font='700 13px Sora,sans-serif';
  cx.textAlign='center'; cx.fillText(c.name,70,170);
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
  });
  // hair swatches
  el.querySelectorAll('[data-col]').forEach(b=>b.addEventListener('click',()=>{
    G.char.hairColor=b.dataset.col; lsSaveChar(); syncSwatches();
  }));
  el.querySelector('#cpHairPick').addEventListener('input',e=>{
    G.char.hairColor=e.target.value; lsSaveChar(); syncSwatches();
  });
  // hair style
  el.querySelectorAll('[data-hs]').forEach(b=>b.addEventListener('click',()=>{
    G.char.hairStyle=b.dataset.hs; lsSaveChar(); syncToggles();
  }));
  // body / breast
  el.querySelector('#cpBody').addEventListener('input',e=>{ G.char.bodyScale=e.target.value/100; lsSaveChar(); });
  el.querySelector('#cpBreast').addEventListener('input',e=>{ G.char.breastSize=e.target.value/100; lsSaveChar(); });
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
