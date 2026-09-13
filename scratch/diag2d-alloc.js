// count allocations per draw() and per-frame shape/fill/clip churn
(function(){
  var C=X; var n={
    radial:0, linear:0, save:0, restore:0, clip:0, fill:0, stroke:0, drawImage:0
  };
  var cr=C.createRadialGradient.bind(C), cl=C.createLinearGradient.bind(C);
  C.createRadialGradient=function(){n.radial++;return cr.apply(null,arguments);};
  C.createLinearGradient=function(){n.linear++;return cl.apply(null,arguments);};
  var sa=C.save.bind(C), re=C.restore.bind(C), cp=C.clip.bind(C), fi=C.fill.bind(C), st=C.stroke.bind(C), di=C.drawImage.bind(C);
  C.save=function(){n.save++;return sa();}; C.restore=function(){n.restore++;return re();};
  C.clip=function(){n.clip++;return cp.apply(null,arguments);}; C.fill=function(){n.fill++;return fi.apply(null,arguments);};
  C.stroke=function(){n.stroke++;return st.apply(null,arguments);}; C.drawImage=function(){n.drawImage++;return di.apply(null,arguments);};
  var out=null;
  try{ draw(); out=n; } finally {
    C.createRadialGradient=cr; C.createLinearGradient=cl; C.save=sa; C.restore=re; C.clip=cp; C.fill=fi; C.stroke=st; C.drawImage=di;
  }
  return JSON.stringify(out);
})();
