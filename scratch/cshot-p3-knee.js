G.pos=3;
(function(){
  function fb(n){var f=null;if(window.GLB_MODEL&&GLB_MODEL.model)GLB_MODEL.model.traverse(function(c){if(c.isBone&&c.name.replace(/[._]/g,'')===n)f=f||c;});return f;}
  var b=fb('ひざL'); if(!b) return 'no-bone';
  b.updateWorldMatrix(true,false);
  var p=new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  CAM3.target.copy(p); CAM3.dist=0.62; CAM3.yaw=0.9; CAM3.pitch=0.22;
  return 'cam @ '+p.toArray().map(function(v){return +v.toFixed(3);}).join(',');
})()
