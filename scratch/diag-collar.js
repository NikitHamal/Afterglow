(function(){
  const rec = [];
  let cur = null;
  // map a LOCAL point through the live canvas transform into screen space
  const T = (x, y) => {
    const m = X.getTransform();
    return [m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f];
  };
  const bb = (x, y) => {
    if(!cur) return;
    const p = T(x, y);
    cur.x0 = Math.min(cur.x0, p[0]); cur.x1 = Math.max(cur.x1, p[0]);
    cur.y0 = Math.min(cur.y0, p[1]); cur.y1 = Math.max(cur.y1, p[1]);
  };
  const oMove = X.moveTo.bind(X), oLine = X.lineTo.bind(X),
        oBez = X.bezierCurveTo.bind(X), oQuad = X.quadraticCurveTo.bind(X),
        oArc = X.arc.bind(X), oEll = X.ellipse.bind(X),
        oBegin = X.beginPath.bind(X), oFill = X.fill.bind(X),
        oFillRect = X.fillRect.bind(X), oStroke = X.stroke.bind(X);
  X.beginPath = function(){ cur = {x0:1e9,x1:-1e9,y0:1e9,y1:-1e9}; return oBegin(); };
  X.moveTo = function(x,y){ bb(x,y); return oMove(x,y); };
  X.lineTo = function(x,y){ bb(x,y); return oLine(x,y); };
  X.bezierCurveTo = function(a,b,c,d,e,f){ bb(a,b);bb(c,d);bb(e,f); return oBez(a,b,c,d,e,f); };
  X.quadraticCurveTo = function(a,b,c,d){ bb(a,b);bb(c,d); return oQuad(a,b,c,d); };
  X.arc = function(x,y,r,a0,a1){ bb(x-r,y-r);bb(x+r,y+r); return oArc(x,y,r,a0,a1); };
  X.ellipse = function(x,y,rx,ry,rot,a0,a1){ bb(x-rx,y-ry);bb(x+rx,y+ry); return oEll(x,y,rx,ry,rot,a0,a1); };
  X.fill = function(){ if(cur) rec.push({op:'fill', ...cur, st:String(X.fillStyle).slice(0,44)}); return oFill(); };
  X.fillRect = function(x,y,w,h){ rec.push({op:'rect',x0:x,x1:x+w,y0:y,y1:y+h,st:String(X.fillStyle).slice(0,44)}); return oFillRect(x,y,w,h); };
  X.stroke = function(){ if(cur) rec.push({op:'stroke', ...cur, st:String(X.strokeStyle).slice(0,44)}); return oStroke(); };

  try { draw(); } catch(e){ return 'draw err: ' + e.message; }
  X.beginPath=oBegin; X.moveTo=oMove; X.lineTo=oLine; X.bezierCurveTo=oBez;
  X.quadraticCurveTo=oQuad; X.arc=oArc; X.ellipse=oEll; X.fill=oFill;
  X.fillRect=oFillRect; X.stroke=oStroke;

  const probe = (px, py, label) => {
    const hits = rec.filter(r => r.x0 <= px && px <= r.x1 && r.y0 <= py && py <= r.y1);
    return label + ' (' + px + ',' + py + ') covered by ' + hits.length + ':\n' +
      hits.slice(-6).map(r => '   ' + r.op.padEnd(6) +
        ' x[' + r.x0.toFixed(0) + '..' + r.x1.toFixed(0) + ']' +
        ' y[' + r.y0.toFixed(0) + '..' + r.y1.toFixed(0) + '] ' + r.st).join('\n');
  };
  // scan for the top edge of the body at the midline
  let edge = null;
  for(let y = 120; y < 320; y += 2){
    const hit = rec.some(r => r.x0 <= 640 && 640 <= r.x1 && r.y0 <= y && y <= r.y1);
    if(hit){ edge = y; break; }
  }
  return 'total ops=' + rec.length + '\nbody top edge at midline y=' + edge + '\n' +
    probe(640, 200, 'midline') + '\n' + probe(560, 230, 'left chest') + '\n' + probe(760, 230, 'right chest');
})()
