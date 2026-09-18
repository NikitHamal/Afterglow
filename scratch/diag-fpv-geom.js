/* diag-fpv-geom.js — report the exact FPV anatomy anchor points so I can see
   how badly the proportions are off relative to the 1280x720 frame. */
(function(){
  const bsz = 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62;
  const bdy = 0.82 + (G.char ? G.char.bodyScale : 0.45) * 0.42;
  const cx = 640, topY = 206;
  const spread = (1.0 + (G.pleasure||0)*0.0016) * bdy;
  const L = [];
  L.push('char=' + (G.char && G.char.id) + ' bsz=' + bsz.toFixed(3) + ' bdy=' + bdy.toFixed(3) + ' spread=' + spread.toFixed(3));
  L.push('frame 1280x720   cx=' + cx + ' topY=' + topY.toFixed(1));
  L.push('-- legs --');
  for(const s of [-1,1]){
    const hip=[cx+s*64*bdy, 556], knee=[cx+s*204*spread, 642], foot=[cx+s*274*spread, 758];
    L.push(' s='+s+' hip=('+hip[0].toFixed(0)+','+hip[1].toFixed(0)+') knee=('+knee[0].toFixed(0)+','+knee[1].toFixed(0)+') foot=('+foot[0].toFixed(0)+','+foot[1].toFixed(0)+')');
    L.push('     thighLen='+Math.hypot(knee[0]-hip[0],knee[1]-hip[1]).toFixed(1)+' shinLen='+Math.hypot(foot[0]-knee[0],foot[1]-knee[1]).toFixed(1)+' (ratio '+(Math.hypot(knee[0]-hip[0],knee[1]-hip[1])/Math.hypot(foot[0]-knee[0],foot[1]-knee[1])).toFixed(2)+')');
  }
  L.push('-- torso --');
  L.push(' chestWidth@335 = ' + (74*bdy*2).toFixed(0) + '  waistWidth@435 = ' + (54*bdy*2).toFixed(0) + '  hipWidth@546 = ' + (82*bdy*2).toFixed(0));
  L.push(' torso height 206..572 = 366   leg hip->foot = ' + Math.hypot(274*spread-64*bdy, 758-556).toFixed(0));
  L.push('-- arms --');
  const a2 = sm(55,80,G.pleasure||0);
  L.push(' a2='+a2.toFixed(3));
  for(const s of [-1,1]){
    const sh=[cx+s*58*bdy,248], el=[cx+s*90*bdy, lerp(352,300,a2)], ha=[lerp(cx+s*116*bdy,cx+s*64*bdy,a2), lerp(462,190,a2)];
    L.push(' s='+s+' sh=('+sh[0].toFixed(0)+','+sh[1].toFixed(0)+') el=('+el[0].toFixed(0)+','+el[1].toFixed(0)+') hand=('+ha[0].toFixed(0)+','+ha[1].toFixed(0)+')');
  }
  L.push('-- breasts --');
  L.push(' bx offset=' + (30*bdy + bsz*4.5).toFixed(1) + ' by=336  baseW=' + (34*bsz).toFixed(1) + ' baseH=' + (40*bsz).toFixed(1));
  L.push(' nipSeparation=' + (2*(30*bdy+bsz*4.5)).toFixed(0) + 'px  breastWidthEach=' + (34*bsz*2).toFixed(0) + 'px');
  L.push('-- vulva --');
  L.push(' vy=554  (torso bottom 572, so vulva sits ABOVE the torso bottom by ' + (572-554) + 'px)');
  L.push('-- viewport --');
  L.push(' visible Y range at zoom=1: 0..720    legs extend to 758 => CUT OFF by 38px');
  return L.join('\n');
})()
