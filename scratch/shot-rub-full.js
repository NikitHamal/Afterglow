G.pos=0;G.spaceHeld=true;G.rub=1;G.rubZone=3;
(function(){
  function fb(n){var f=null;if(window.GLB_MODEL&&GLB_MODEL.model)GLB_MODEL.model.traverse(function(c){if(c.isBone&&c.name.replace(/[._]/g,'')===n)f=f||c;});return f;}
  var b=fb('下半身'); if(!b) return 'no-bone';
  b.updateWorldMatrix(true,false);
  var p=new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  CAM3.target.copy(p); CAM3.dist=1.25; CAM3.yaw=0.7; CAM3.pitch=0.25;
  return 'cam @ '+p.toArray().map(function(v){return +v.toFixed(3);}).join(',');
})()
