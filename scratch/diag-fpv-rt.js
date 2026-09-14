(function(){
  const B = FPV_BODY, C = FPV_CAM;
  const bdy = fpvBdy();
  const out = [];
  const P = (n,u,d,h) => { const p = fpvProj(u,d,h); out.push([n, p[0], p[1], p[2]]); return p; };

  out.push(['CAM', C.cx, C.topY, C.k, C.kpad, C.dOff, C.eyeH]);
  const hipHalf=B.hipHalf*bdy, shHalf=B.shHalf*bdy;
  P('toeL',      B.footSpread*bdy*1.08, 0.20, 0.11);
  P('ankleL',    B.footSpread*bdy,      B.ankle, 0.15);
  P('kneeL',    (B.kneeSpread+0.05)*bdy, B.knee, B.lift+0.14);
  P('hipL',      hipHalf,  B.hip,   B.lift);
  P('mons',      0,        B.mons,  B.lift+0.04);
  P('navel',     0,        B.navel, B.lift+0.015);
  P('waist',     0,        B.waist, B.lift+0.004);
  P('ribs',      0,        B.ribs,  B.lift);
  P('stern',     0,        B.sternum, B.lift+0.006);
  P('shL',       shHalf,   B.shoulder, B.lift);
  P('shR',      -shHalf,   B.shoulder, B.lift);
  P('breastL',   0.132*bdy, B.sternum-0.03, B.lift+0.030);
  P('chin',      0,        B.chin+0.02, B.lift+0.055);
  P('crown',     0,        B.crown, B.lift+0.06);
  P('elbowL',    0.232*bdy, B.shoulder-0.30, B.lift-0.075);
  P('wristL',    0.212*bdy, B.shoulder-0.56, B.lift-0.065);

  // head block
  const hp = fpvProj(0, B.chin+0.02, B.lift+0.055);
  const hs = (hp[2]/430)*0.44;
  const squashY = 0.86;
  const hy = hp[1];
  const _sy = hs*squashY, _sx = hs;
  const shLp = fpvProj(B.shHalf*fpvBdy(), B.shoulder, B.lift);
  const nkBaseY = ((shLp[1]+shLp[1])/2 - hy)/_sy;
  const nkHalfPx = 0.055*fpvS(B.neck)*fpvBdy();
  const nkHalfN = nkHalfPx/_sx;
  const clavY = (shLp[1]+shLp[1])/2 + 0.012*P('tmp',0,B.sternum,B.lift)[2];
  out.push(['head hy', hy, hs, squashY, 0]);
  out.push(['neck nkBaseY', nkBaseY, 'nkBot='+Math.max(46,Math.min(280,nkBaseY)), 0, 0]);
  out.push(['neck halfPx', nkHalfPx, 'local='+nkHalfN, 'screenW='+(2*nkHalfN*_sx), 0]);
  out.push(['neck flare base screenW', 2*nkHalfN*1.86*_sx, 'torso clavY='+clavY, 0, 0]);
  out.push(['neck top screen y', hy+20*_sy, 'neck bot screen y', hy+Math.min(280,nkBaseY)*_sy, 0]);
  out.push(['head hair mass W', 160*hs, 'face W', 88*hs, 0, 0]);
  out.push(['torso width@waist', 2*(B.waistHalf*bdy)*fpvProj(0,B.waist,0)[2], 0, 0, 0]);

  return out.map(r => r.map(v => typeof v === 'number' ? (+v).toFixed(1) : v).join(' | ')).join('\n');
})()
