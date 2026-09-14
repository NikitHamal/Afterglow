(function(){
  const s = getSkin();
  const T = herT();
  return [
    'char=' + (G.char && G.char.preset) + ' skinTone=' + (G.char && G.char.skinTone),
    'getSkin.her   = ' + s.her,
    'getSkin.herSh = ' + s.herSh,
    'herT().hi     = ' + T.hi,
    'herT().b      = ' + T.b,
    'herT().s      = ' + T.s,
    'herT().d      = ' + T.d,
    'skTone(s.her).b = ' + skTone(s.her).b,
    'gLinear exists = ' + (typeof gLinear),
    'sg exists = ' + (typeof sg)
  ].join('\n');
})()
