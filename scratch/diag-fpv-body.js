/* diag-fpv-body.js — report the projected skeleton the new drawFPVBody uses,
   so I can see exact screen coordinates and radii. */
(function(){
  const B = FPV_BODY;
  const bsz = 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62;
  const bdy = 0.82 + (G.char ? G.char.bodyScale : 0.45) * 0.42;
  const ple = G.pleasure || 0;
  const drawUp  = 0.28 + ple * 0.0026;
  const openAmt = 0.055 + ple * 0.0011;
  const hipHalf = B.hipHalf*bdy, shHalf = B.shHalf*bdy, waistHalf=B.waistHalf*bdy, ribHalf=B.ribHalf*bdy;
  const R = (base, sc) => base * sc * 0.0016;

  const P = {};
  P.hipL  = fpvProj(-hipHalf,  B.hip,     B.lift);
  P.hipR  = fpvProj( hipHalf,  B.hip,     B.lift);
  P.mons  = fpvProj(0, B.mons,  B.lift + 0.04);
  P.navel = fpvProj(0, B.navel, B.lift + 0.015);
  P.waist = fpvProj(0, B.waist, B.lift + 0.004);
  P.ribs  = fpvProj(0, B.ribs,  B.lift);
  P.stern = fpvProj(0, B.sternum, B.lift + 0.006);
  P.shL   = fpvProj( shHalf, B.shoulder, B.lift);
  P.shR   = fpvProj(-shHalf, B.shoulder, B.lift);
  P.kneeL = fpvProj( (B.kneeSpread + openAmt)*bdy, B.knee, B.lift + drawUp);
  P.kneeR = fpvProj(-(B.kneeSpread + openAmt)*bdy, B.knee, B.lift + drawUp);
  P.ankL  = fpvProj( B.footSpread*bdy, B.ankle, 0.15);
  P.ankR  = fpvProj(-B.footSpread*bdy, B.ankle, 0.15);
  P.toeL  = fpvProj( B.footSpread*bdy*1.08, 0.20, 0.11);
  P.toeR  = fpvProj(-B.footSpread*bdy*1.08, 0.20, 0.11);

  const L = [];
  L.push('bdy=' + bdy.toFixed(3) + ' bsz=' + bsz.toFixed(3));
  for(const k of Object.keys(P)){
    const p = P[k];
    L.push(k.padEnd(6) + ' (' + p[0].toFixed(0).padStart(5) + ',' + p[1].toFixed(0).padStart(5) + ')  sc=' + p[2].toFixed(0));
  }
  L.push('');
  L.push('-- limb radii in px --');
  L.push('thigh@hip  R(48,hip)=' + R(48,P.hipL[2]).toFixed(1) + '   thigh@knee R(40,knee)=' + R(40,P.kneeL[2]).toFixed(1));
  L.push('calf@knee  R(38,knee)=' + R(38,P.kneeL[2]).toFixed(1) + '   calf@ank  R(24,ank)=' + R(24,P.ankL[2]).toFixed(1));
  L.push('head half-width 0.092m -> ' + (0.092*P.stern[2]).toFixed(1) + 'px');
  L.push('');
  L.push('-- projected lengths --');
  L.push('thigh screen len = ' + Math.hypot(P.kneeL[0]-P.hipL[0], P.kneeL[1]-P.hipL[1]).toFixed(0) + 'px');
  L.push('calf  screen len = ' + Math.hypot(P.ankL[0]-P.kneeL[0], P.ankL[1]-P.kneeL[1]).toFixed(0) + 'px');
  L.push('torso screen len = ' + Math.hypot(P.stern[0]-P.mons[0], P.stern[1]-P.mons[1]).toFixed(0) + 'px');
  L.push('torso width @ribs = ' + (2*ribHalf*P.ribs[2]).toFixed(0) + 'px @waist=' + (2*waistHalf*P.waist[2]).toFixed(0) + 'px');
  L.push('breast centred at u=' + (0.145*bdy).toFixed(3) + 'm -> ' + (0.145*bdy*P.stern[2]).toFixed(0) + 'px from centre');
  L.push('breast baseW=' + R(40*bsz, P.stern[2]).toFixed(1) + 'px baseH=' + R(46*bsz, P.stern[2]).toFixed(1) + 'px');
  L.push('');
  L.push('visible frame 0..1280 x 0..720');
  return L.join('\n');
})()
