// Afterglow 3D — module: post3d (hand-rolled HDR post chain)
// scene → HalfFloat MSAA target → bright pass → 5-level blur pyramid →
// composite (bloom, ACES, warm grade, vignette, grain, edge aberration).
// renderPost3() returns false when unavailable so app3d falls back to a
// plain renderer3d.render(). ?post=0 disables it for A/B comparisons.
'use strict';

const POST3 = {
  on: false, w: 0, h: 0, levels: 5,
  rt: null, bright: null, blurA: [], blurB: [],
  quad: null, scene: null, cam: null,
  mBright: null, mBlur: null, mFinal: null,
  t: 0, flash: 0, pulse: 0, lastSpurts: 0, lastOrg: 0
};

const _POST_VS =
  'varying vec2 vUv;\n' +
  'void main(){ vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }';

function _postTarget3(w, h, samples, depth) {
  const rt = new THREE.WebGLRenderTarget(Math.max(1, w), Math.max(1, h), {
    type: THREE.HalfFloatType, format: THREE.RGBAFormat,
    minFilter: THREE.LinearFilter, magFilter: THREE.LinearFilter,
    depthBuffer: !!depth, stencilBuffer: false
  });
  if (samples) rt.samples = samples;
  rt.texture.generateMipmaps = false;
  return rt;
}

function initPost3() {
  if (!renderer3d || POST3.on) return;
  try {
    if (/[?&]post=0/.test(location.search)) return;
    const caps = renderer3d.capabilities;
    const ok = caps.isWebGL2
      ? (renderer3d.extensions.has('EXT_color_buffer_float') || renderer3d.extensions.has('EXT_color_buffer_half_float'))
      : (renderer3d.extensions.has('OES_texture_half_float') && renderer3d.extensions.has('OES_texture_half_float_linear'));
    if (!ok) return;

    POST3.scene = new THREE.Scene();
    POST3.cam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);
    POST3.quad = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), null);
    POST3.quad.frustumCulled = false;
    POST3.scene.add(POST3.quad);

    POST3.mBright = new THREE.ShaderMaterial({
      uniforms: { tSrc: { value: null }, uThresh: { value: 0.9 }, uKnee: { value: 0.6 } },
      vertexShader: _POST_VS,
      fragmentShader:
        'uniform sampler2D tSrc; uniform float uThresh; uniform float uKnee; varying vec2 vUv;\n' +
        'void main(){ vec3 c = texture2D(tSrc, vUv).rgb;' +
        ' float l = max(c.r, max(c.g, c.b));' +
        ' float soft = clamp(l - uThresh + uKnee, 0.0, 2.0*uKnee); soft = soft*soft/(4.0*uKnee + 1e-4);' +
        ' float k = max(soft, l - uThresh) / max(l, 1e-4);' +
        ' gl_FragColor = vec4(min(c * k, vec3(24.0)), 1.0); }',
      depthTest: false, depthWrite: false
    });
    // 9-tap gaussian via 5 linear-filtered fetches
    POST3.mBlur = new THREE.ShaderMaterial({
      uniforms: { tSrc: { value: null }, uDir: { value: new THREE.Vector2() } },
      vertexShader: _POST_VS,
      fragmentShader:
        'uniform sampler2D tSrc; uniform vec2 uDir; varying vec2 vUv;\n' +
        'void main(){ vec3 c = texture2D(tSrc, vUv).rgb * 0.2270270270;' +
        ' c += texture2D(tSrc, vUv + uDir*1.3846153846).rgb * 0.3162162162;' +
        ' c += texture2D(tSrc, vUv - uDir*1.3846153846).rgb * 0.3162162162;' +
        ' c += texture2D(tSrc, vUv + uDir*3.2307692308).rgb * 0.0702702703;' +
        ' c += texture2D(tSrc, vUv - uDir*3.2307692308).rgb * 0.0702702703;' +
        ' gl_FragColor = vec4(c, 1.0); }',
      depthTest: false, depthWrite: false
    });
    POST3.mFinal = new THREE.ShaderMaterial({
      uniforms: {
        tScene: { value: null }, tB0: { value: null }, tB1: { value: null }, tB2: { value: null }, tB3: { value: null }, tB4: { value: null },
        uRes: { value: new THREE.Vector2(1, 1) }, uTime: { value: 0 },
        uExposure: { value: 1.0 }, uBloom: { value: 0.6 }, uVig: { value: 0.55 }, uGrain: { value: 0.035 },
        uCA: { value: 0.0035 }, uHeat: { value: 0 }, uFlash: { value: 0 }, uPulse: { value: 0 }
      },
      vertexShader: _POST_VS,
      fragmentShader: [
        'uniform sampler2D tScene, tB0, tB1, tB2, tB3, tB4;',
        'uniform vec2 uRes; uniform float uTime, uExposure, uBloom, uVig, uGrain, uCA, uHeat, uFlash, uPulse;',
        'varying vec2 vUv;',
        'vec3 agRRTFit(vec3 v){ vec3 a = v*(v + 0.0245786) - 0.000090537; vec3 b = v*(0.983729*v + 0.4329510) + 0.238081; return a/b; }',
        'vec3 agAces(vec3 c){',
        '  const mat3 I = mat3(vec3(0.59719,0.07600,0.02840), vec3(0.35458,0.90834,0.13383), vec3(0.04823,0.01566,0.83777));',
        '  const mat3 O = mat3(vec3(1.60475,-0.10208,-0.00327), vec3(-0.53108,1.10813,-0.07276), vec3(-0.07367,-0.00605,1.07602));',
        '  c = I * (c / 0.6); c = agRRTFit(c); return clamp(O * c, 0.0, 1.0); }',
        'vec3 toSRGB(vec3 c){ return mix(c*12.92, 1.055*pow(c, vec3(1.0/2.4)) - 0.055, step(vec3(0.0031308), c)); }',
        'float hash(vec2 p){ p = fract(p*vec2(443.897, 441.423)); p += dot(p, p.yx + 19.19); return fract((p.x + p.y)*p.x); }',
        'void main(){',
        '  vec2 uv = vUv, d = uv - 0.5;',
        '  float r2 = dot(d, d);',
        // heat haze: a faint shimmer that only shows up as she gets close
        '  uv += vec2(sin(uv.y*38.0 + uTime*2.3), cos(uv.x*31.0 - uTime*1.9)) * 0.0011 * uHeat * uHeat;',
        '  vec2 off = d * r2 * uCA * 4.0;',
        '  vec3 col = vec3(texture2D(tScene, uv - off).r, texture2D(tScene, uv).g, texture2D(tScene, uv + off).b);',
        '  vec3 bl = texture2D(tB0, uv).rgb*0.32 + texture2D(tB1, uv).rgb*0.26 + texture2D(tB2, uv).rgb*0.20 + texture2D(tB3, uv).rgb*0.13 + texture2D(tB4, uv).rgb*0.09;',
        '  col += bl * uBloom;',
        // warm the light that is there, cool the dark — split-tone in linear
        '  float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));',
        '  vec3 warm = vec3(1.06 + 0.06*uHeat, 0.98, 0.90 - 0.05*uHeat);',
        '  vec3 cool = vec3(0.92, 0.94, 1.10);',
        '  col *= mix(cool, warm, smoothstep(0.02, 0.35, lum));',
        '  col *= uExposure * (1.0 + uPulse*0.12);',
        '  col = agAces(col);',
        '  col = toSRGB(col);',
        // gentle filmic contrast + saturation, plum-lifted blacks
        '  col = mix(col, col*col*(3.0 - 2.0*col), 0.22);',
        '  float g = dot(col, vec3(0.299, 0.587, 0.114));',
        '  col = mix(vec3(g), col, 1.06 + 0.12*uHeat);',
        '  col = col + vec3(0.020, 0.008, 0.018) * (1.0 - g);',
        // vignette: tightens with heat, breathes with the pulse
        '  float vr = length(d * vec2(uRes.x/uRes.y, 1.0) * 0.82);',
        '  float vig = smoothstep(0.95 - uVig*0.25 - uHeat*0.12 - uPulse*0.06, 0.18, vr);',
        '  col *= mix(1.0, vig, uVig);',
        '  col += vec3(0.20, 0.03, 0.08) * (1.0 - vig) * uHeat * 0.35;',  // rose edge flush near climax
        '  float n = hash(vUv*uRes + fract(uTime*37.0)*91.0) - 0.5;',
        '  col += n * uGrain * (1.0 - g*0.6);',
        '  col = mix(col, vec3(1.0, 0.95, 0.93), clamp(uFlash, 0.0, 1.0));',
        '  gl_FragColor = vec4(col, 1.0);',
        '}'
      ].join('\n'),
      depthTest: false, depthWrite: false
    });
    POST3.on = true;
    const st = document.getElementById('stage');
    resizePost3(st ? st.clientWidth : window.innerWidth, st ? st.clientHeight : window.innerHeight);
  } catch (e) {
    POST3.on = false;
    if (typeof console !== 'undefined') console.warn('post3: disabled', e);
  }
}

function resizePost3(w, h) {
  if (!POST3.on || !renderer3d) return;
  const pr = renderer3d.getPixelRatio();
  const W = Math.max(1, Math.round(w * pr)), H = Math.max(1, Math.round(h * pr));
  if (W === POST3.w && H === POST3.h) return;
  POST3.w = W; POST3.h = H;
  const dispose = rt => rt && rt.dispose();
  dispose(POST3.rt); dispose(POST3.bright);
  POST3.blurA.forEach(dispose); POST3.blurB.forEach(dispose);
  // high pixel ratios already supersample: 2x MSAA there halves the HDR target's memory and bandwidth
  const samples = renderer3d.capabilities.isWebGL2 ? (pr >= 1.5 ? 2 : 4) : 0;
  POST3.rt = _postTarget3(W, H, samples, true);
  POST3.bright = _postTarget3(W >> 1, H >> 1, 0);
  POST3.blurA = []; POST3.blurB = [];
  for (let i = 0; i < POST3.levels; i++) {
    const lw = Math.max(1, W >> (i + 1)), lh = Math.max(1, H >> (i + 1));
    POST3.blurA.push(_postTarget3(lw, lh, 0));
    POST3.blurB.push(_postTarget3(lw, lh, 0));
  }
  POST3.mFinal.uniforms.uRes.value.set(W, H);
}

function _pass3(mat, target) {
  POST3.quad.material = mat;
  renderer3d.setRenderTarget(target);
  renderer3d.render(POST3.scene, POST3.cam);
}

/* game-state → look. Bloom swells with arousal and every spurt/orgasm. */
function _postDrive3(dt) {
  const U = POST3.mFinal.uniforms;
  const g = (typeof G !== 'undefined') ? G : {};
  const heat = clamp((g.pleasure || 0) / 100, 0, 1);
  if ((g.spurts || 0) > POST3.lastSpurts) POST3.pulse = 1;
  POST3.lastSpurts = g.spurts || 0;
  if ((g.orgasms || 0) > POST3.lastOrg) { POST3.pulse = 1; POST3.flash = Math.max(POST3.flash, 0.35); }
  POST3.lastOrg = g.orgasms || 0;
  POST3.pulse *= Math.pow(0.02, dt);
  POST3.flash *= Math.pow(0.004, dt);
  const orgGlow = clamp(g.orgT || 0, 0, 1);
  POST3.t += dt;
  U.uTime.value = POST3.t;
  U.uHeat.value = lerp(U.uHeat.value, heat, 1 - Math.pow(0.2, dt));
  U.uBloom.value = 0.4 + heat * 0.3 + (g.bloom || 0) * 0.8 + orgGlow * 0.6 + POST3.pulse * 0.6 + (g.climax ? 0.3 : 0);
  U.uPulse.value = POST3.pulse + orgGlow * 0.5;
  U.uFlash.value = POST3.flash;
  U.uExposure.value = 0.78 + heat * 0.06;
  U.uVig.value = 0.6;
  // a heartbeat in the threshold so highlights throb when she's close
  POST3.mBright.uniforms.uThresh.value = 0.85 - heat * 0.2 - Math.max(0, Math.sin(POST3.t * 7.5)) * heat * heat * 0.08;
}

function renderPost3(dt) {
  if (!POST3.on || !POST3.rt) return false;
  _postDrive3(dt || 0.016);
  renderer3d.setRenderTarget(POST3.rt);
  renderer3d.render(scene3d, camera3d);

  POST3.mBright.uniforms.tSrc.value = POST3.rt.texture;
  _pass3(POST3.mBright, POST3.bright);
  let src = POST3.bright;
  for (let i = 0; i < POST3.levels; i++) {
    const a = POST3.blurA[i], b = POST3.blurB[i];
    POST3.mBlur.uniforms.tSrc.value = src.texture;
    POST3.mBlur.uniforms.uDir.value.set(1 / a.width, 0);
    _pass3(POST3.mBlur, a);
    POST3.mBlur.uniforms.tSrc.value = a.texture;
    POST3.mBlur.uniforms.uDir.value.set(0, 1 / a.height);
    _pass3(POST3.mBlur, b);
    src = b;
  }
  const U = POST3.mFinal.uniforms;
  U.tScene.value = POST3.rt.texture;
  U.tB0.value = POST3.blurB[0].texture; U.tB1.value = POST3.blurB[1].texture;
  U.tB2.value = POST3.blurB[2].texture; U.tB3.value = POST3.blurB[3].texture;
  U.tB4.value = POST3.blurB[4].texture;
  _pass3(POST3.mFinal, null);
  return true;
}

if (typeof window !== 'undefined') { window.POST3 = POST3; }
