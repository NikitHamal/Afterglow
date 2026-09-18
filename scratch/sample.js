// Sample skin luminance across the FPV torso to quantify form shading.
(function(){
  const d = X.getImageData(0, 0, W, H).data;
  const lum = (x, y) => { const i = ((y|0) * W + (x|0)) * 4; return Math.round(0.2126*d[i] + 0.7152*d[i+1] + 0.0722*d[i+2]); };
  const rowAt = (y) => {
    const out = [];
    for (let x = 480; x <= 800; x += 40) out.push(x + ':' + lum(x, y));
    return y + ' => ' + out.join(' ');
  };
  const sclera = { eyeL: [], };
  return {
    rows: [400, 470, 560, 640, 700].map(rowAt),
    // vertical scan down the trunk centre-left and centre-right to spot banding
    colLeft: [230,300,380,460,540,620].map(y => y + ':' + lum(600, y)),
    colRight: [230,300,380,460,540,620].map(y => y + ':' + lum(690, y)),
    neck: [110,140,170,200].map(y => y + ':' + lum(641, y)),
    headCheek: lum(600, 150) + ' ' + lum(660, 150)
  };
})()
