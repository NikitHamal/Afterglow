(function(){
  function fb(n){var f=null;if(window.GLB_MODEL&&GLB_MODEL.model)GLB_MODEL.model.traverse(function(c){if(c.isBone&&c.name.replace(/[._]/g,'')===n)f=f||c;});return f;}
  var b=fb('つま先L'); if(!b) return 'no-bone:'+'つま先L';
  b.updateWorldMatrix(true,false);
  var p=new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  CAM3.target.copy(p); CAM3.dist=0.26; CAM3.yaw=1.0; CAM3.pitch=0.3;
  return 'つま先L'+' @ '+p.toArray().map(function(v){return +v.toFixed(3);}).join(',');
})()