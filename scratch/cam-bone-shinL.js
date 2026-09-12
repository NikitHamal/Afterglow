(function(){
  function fb(n){var f=null;if(window.GLB_MODEL&&GLB_MODEL.model)GLB_MODEL.model.traverse(function(c){if(c.isBone&&c.name.replace(/[._]/g,'')===n)f=f||c;});return f;}
  var b=fb('ひざL'); if(!b) return 'no-bone:'+'ひざL';
  b.updateWorldMatrix(true,false);
  var p=new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  CAM3.target.copy(p); CAM3.dist=0.42; CAM3.yaw=1.35; CAM3.pitch=0.1;
  return 'ひざL'+' @ '+p.toArray().map(function(v){return +v.toFixed(3);}).join(',');
})()