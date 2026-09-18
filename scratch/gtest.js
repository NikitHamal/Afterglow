// Decisive test: is a CanvasGradient's geometry interpreted in the user space at
// PAINT time (then one object can be shared by every shade() blob) or captured at
// CREATION time (then it cannot)? Difference in one byte decides a big optimisation.
(function(){
  const c = document.createElement('canvas');
  c.width = 64; c.height = 64;
  const g2 = c.getContext('2d');
  const mk = () => {
    const g = g2.createRadialGradient(0, 0, 0, 0, 0, 1);
    g.addColorStop(0, 'rgba(255,255,255,1)');
    g.addColorStop(1, 'rgba(255,255,255,0)');
    return g;
  };
  g2.clearRect(0, 0, 64, 64);
  const ga = mk();                       // created under IDENTITY
  g2.save(); g2.translate(32, 32); g2.scale(24, 24);
  g2.fillStyle = ga; g2.beginPath(); g2.arc(0, 0, 1, 0, Math.PI * 2); g2.fill();
  g2.restore();
  const a = g2.getImageData(32, 32, 1, 1).data[0];
  g2.clearRect(0, 0, 64, 64);
  g2.save(); g2.translate(32, 32); g2.scale(24, 24);
  const gb = mk();                       // created under the SAME transform
  g2.fillStyle = gb; g2.beginPath(); g2.arc(0, 0, 1, 0, Math.PI * 2); g2.fill();
  g2.restore();
  const b = g2.getImageData(32, 32, 1, 1).data[0];
  return { createdUnderIdentity: a, createdUnderTransform: b, shareable: a === b && a > 200 };
})()
