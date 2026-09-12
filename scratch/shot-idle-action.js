G.pos=0;G.rub=0;G.rubZone=0;G.spaceHeld=false;
(function(){
  function fb(n){var f=null;if(window.GLB_MODEL&&GLB_MODEL.model)GLB_MODEL.model.traverse(function(c){if(c.isBone&&c.name.replace(/[._]/g,'')===n)f=f||c;});return f;}
  var a=fb('手首L'), b=fb('手首R');
  if(!a||!b) return 'no-bone';
  a.updateWorldMatrix(true,false); b.updateWorldMatrix(true,false);
  var p=new THREE.Vector3().setFromMatrixPosition(a.matrixWorld);
  var q=new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  CAM3.target.copy(p).add(q).multiplyScalar(0.5);
  CAM3.dist=0.85; CAM3.yaw=0.95; CAM3.pitch=0.45;
  return 'mid-hands @ '+CAM3.target.toArray().map(function(v){return +v.toFixed(3);}).join(',');
})()
