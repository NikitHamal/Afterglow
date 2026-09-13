(function(){
  function fb(n){var f=null;if(window.GLB_MODEL&&GLB_MODEL.model)GLB_MODEL.model.traverse(function(c){if(c.isBone&&c.name.replace(/[._]/g,'')===n)f=f||c;});return f;}
  var b=fb('ひじL'); if(!b) return 'no-bone:'+'ひじL';
  b.updateWorldMatrix(true,false);
  var p=new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  CAM3.target.copy(p); CAM3.dist=0.45; CAM3.yaw=0.8; CAM3.pitch=0.15;
  return 'ひじL'+' @ '+p.toArray().map(function(v){return +v.toFixed(3);}).join(',');
})()