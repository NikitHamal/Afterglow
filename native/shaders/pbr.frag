#version 330 core
// Forward PBR shading for Afterglow. Modes: 0 generic, 1 skin, 2 hair, 3 eye, 4 room, 5 unlit.
// Output: linear HDR radiance (tonemapped later).
in vec3 v_wpos;
in vec3 v_nrm;
in vec4 v_tan;
in vec2 v_uv0;
in vec2 v_uv1;
out vec4 o_color;

uniform int u_mode;
uniform int u_pass;               // 0 opaque / alpha-test, 1 soft hair edges (blended)
uniform vec4 u_baseColor;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_normalScale;
uniform float u_occStrength;
uniform vec3 u_emissive;
uniform sampler2D u_texBase;
uniform sampler2D u_texNormal;
uniform sampler2D u_texMR;
uniform sampler2D u_texOcc;
uniform sampler2D u_texEmis;
uniform int u_hasBase;
uniform int u_hasNormal;
uniform int u_hasMR;
uniform int u_hasOcc;
uniform int u_hasEmis;
uniform int u_uvBase;
uniform int u_uvNormal;
uniform int u_uvMR;
uniform int u_uvOcc;
uniform int u_uvEmis;
uniform int u_alphaMode;          // 0 opaque, 1 mask, 2 blend
uniform float u_cutoff;
uniform int u_doubleSided;
uniform vec3 u_tint;
uniform int u_hairShape;          // hair: bend card normals around the skull centre
uniform vec3 u_hairCenter;
uniform float u_flush;
uniform float u_sweat;
uniform int u_baked;              // room: emissive(uv1) / unlit carries baked light
uniform vec3 u_bakeScale;         // lightmap scale x lighting mood
uniform sampler2D u_skinMask;     // content skin support map (linear): R flush, G translucency, B sweat
uniform int u_hasSkinMask;
uniform sampler2D u_detailN;      // tileable pore/crosshatch normal (uv0 * 48)
uniform int u_hasDetailN;
vec3 g_mask = vec3(0.6, 0.4, 0.5);
uniform vec3 u_camPos;
uniform float u_time;

uniform vec3 u_keyPos;
uniform vec3 u_keyDir;
uniform vec3 u_keyColor;
uniform float u_keyIntensity;
uniform float u_keyCosInner;
uniform float u_keyCosOuter;
uniform float u_keyRange;
uniform sampler2D u_shadowMap;
uniform mat4 u_shadowVP;
uniform int u_shadowOn;
uniform float u_shadowTexel;
uniform float u_shadowSoft;

uniform vec3 u_moonDir;
uniform vec3 u_moonColor;
uniform float u_moonIntensity;
uniform vec3 u_fillDir;
uniform vec3 u_fillColor;
uniform float u_fillIntensity;
uniform vec3 u_skyColor;
uniform vec3 u_groundColor;
uniform float u_ambient;

uniform sampler2D u_ssao;
uniform int u_ssaoOn;
uniform vec2 u_screenSize;

uniform int u_numPoints;
uniform vec3 u_pointPos[6];
uniform vec3 u_pointColor[6];
uniform float u_pointIntensity[6];
uniform float u_pointRange[6];

const float PI = 3.14159265359;

vec2 uvSel(int i) { return i == 1 ? v_uv1 : v_uv0; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float ign(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

float D_GGX(float NoH, float a) { float a2 = a * a; float d = NoH * NoH * (a2 - 1.0) + 1.0; return a2 / (PI * d * d + 1e-7); }
float V_Smith(float NoV, float NoL, float a) {
    float a2 = a * a;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-5);
}
vec3 F_Schlick(float VoH, vec3 f0) { return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0); }

const vec2 POISSON[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725), vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464), vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420), vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590), vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

float shadowFactor(vec3 wpos, vec3 n, vec3 l) {
    if (u_shadowOn == 0) return 1.0;
    float nl = sat(dot(n, l));
    vec3 p = wpos + n * (0.002 + 0.006 * (1.0 - nl));
    vec4 lp = u_shadowVP * vec4(p, 1.0);
    vec3 c = lp.xyz / lp.w * 0.5 + 0.5;
    if (c.z >= 1.0 || c.x <= 0.0 || c.x >= 1.0 || c.y <= 0.0 || c.y >= 1.0) return 1.0;
    float ang = ign(gl_FragCoord.xy) * 6.2831853;
    mat2 R = mat2(cos(ang), sin(ang), -sin(ang), cos(ang));
    float bias = 0.00035 + 0.0009 * (1.0 - nl);
    float radius = u_shadowTexel * u_shadowSoft;
    float sum = 0.0;
    for (int i = 0; i < 16; ++i) {
        float d = texture(u_shadowMap, c.xy + R * POISSON[i] * radius).r;
        sum += (c.z - bias <= d) ? 1.0 : 0.0;
    }
    return sum / 16.0;
}

struct Surf {
    vec3 N, V, T, B, albedo, f0;
    float rough, metal, ao, alpha, strand;
    int mode;
};

// direct light response for one light (L toward light, radiance already attenuated)
vec3 shade(Surf s, vec3 L, vec3 radiance) {
    vec3 H = normalize(L + s.V);
    float NoLraw = dot(s.N, L);
    float NoL = sat(NoLraw), NoV = max(dot(s.N, s.V), 1e-4), NoH = sat(dot(s.N, H)), VoH = sat(dot(s.V, H));
    if (s.mode == 2) {
        // Kajiya-Kay with shifted lobes (Scheuermann) along the strand (s.T): R = narrow, uncoloured;
        // TRT = broader, tinted by the fibre colour (sqrt keeps dark hair from going flat black)
        float shiftN = (hash12(vec2(floor(v_uv0.x * 512.0), 7.0)) - 0.5) * 0.16;
        vec3 T1 = normalize(s.T + s.N * (-0.05 + shiftN));
        vec3 T2 = normalize(s.T + s.N * (0.09 + shiftN));
        float th1 = dot(T1, H), th2 = dot(T2, H);
        float sin1 = sqrt(max(0.0, 1.0 - th1 * th1)), sin2 = sqrt(max(0.0, 1.0 - th2 * th2));
        float dirAtten = smoothstep(-1.0, 0.0, th1);
        vec3 trt = sqrt(max(s.albedo, vec3(1e-4)));
        vec3 spec = vec3(0.05) * pow(sin1, 160.0) * dirAtten + trt * 0.11 * pow(sin2, 44.0) * s.strand;
        float wrap = sat((NoLraw + 0.4) / 1.4);
        vec3 diff = s.albedo / PI * wrap;
        return (diff + spec * sat(NoLraw * 0.7 + 0.3)) * radiance;
    }
    float a = max(s.rough * s.rough, 0.002);
    vec3 F = F_Schlick(VoH, s.f0);
    vec3 spec = D_GGX(NoH, a) * V_Smith(NoV, NoL, a) * F;
    if (s.mode == 1) {
        // skin: second broader lobe + per-channel wrapped diffuse (pre-integrated scattering look)
        float a2 = max(min(s.rough * 1.45, 1.0) * min(s.rough * 1.45, 1.0), 0.002);
        vec3 spec2 = D_GGX(NoH, a2) * V_Smith(NoV, NoL, a2) * F;
        // skin is never plastic: a soft sheen, wetter only where people actually sweat (mask.b)
        spec = mix(spec, spec2, 0.38) * 0.82 * (1.0 + 0.55 * u_sweat * g_mask.b);
        vec3 w = vec3(0.52, 0.30, 0.22);
        vec3 d = clamp((vec3(NoLraw) + w) / (1.0 + w), 0.0, 1.0);
        d = d * d * (3.0 - 2.0 * d) * 0.5 + d * 0.5;
        vec3 diff = s.albedo / PI * d * (1.0 - F);
        // light bleeding through thin parts (ears, fingers, nose) when back-lit
        vec3 trans = s.albedo * vec3(1.0, 0.38, 0.22) * pow(sat(dot(s.V, -L)), 6.0) * 0.18 * (1.0 - NoL) * (g_mask.g * 2.5);
        return (diff + trans + spec * NoL) * radiance;
    }
    vec3 kd = (1.0 - F) * (1.0 - s.metal);
    return (kd * s.albedo / PI + spec) * NoL * radiance;
}

void main() {
    vec4 texS = u_hasBase == 1 ? texture(u_texBase, uvSel(u_uvBase)) : vec4(1.0);
    vec4 base = u_baseColor * texS;
    base.rgb *= u_tint;
    float alpha = base.a;
    if (u_alphaMode == 1) {
        if (u_pass == 0 && alpha < u_cutoff) discard;
        if (u_pass == 1 && (alpha >= u_cutoff || alpha < 0.04)) discard;
    }

    vec3 Ng = normalize(v_nrm);
    if (u_doubleSided == 1 && !gl_FrontFacing) Ng = -Ng;
    vec3 Tg = normalize(v_tan.xyz - Ng * dot(Ng, v_tan.xyz));
    vec3 Bg = cross(Ng, Tg) * v_tan.w;
    vec3 N = Ng;
    if (u_mode == 1 && u_hasSkinMask == 1) g_mask = texture(u_skinMask, v_uv0).rgb;
    if (u_hasNormal == 1) {
        vec3 tn = texture(u_texNormal, uvSel(u_uvNormal)).xyz * 2.0 - 1.0;
        tn.xy *= u_normalScale;
        if (u_mode == 1 && u_hasDetailN == 1) {
            // pores + fine lines, only up close (fades out with distance), smoothed where skin is wet
            float near = 1.0 - smoothstep(0.6, 2.4, length(u_camPos - v_wpos));
            vec2 d = texture(u_detailN, v_uv0 * 48.0).xy * 2.0 - 1.0;
            tn.xy += d * 0.35 * near * (1.0 - g_mask.b * u_sweat);
        }
        N = normalize(mat3(Tg, Bg, Ng) * tn);
    }
    if (u_mode == 1 && u_sweat > 0.01) {
        // sweat beads: sparse micro bumps in world space
        vec3 q = v_wpos * 220.0;
        float h = hash12(floor(q.xy) + floor(q.z) * 17.0);
        float bead = smoothstep(0.92, 1.0, h) * u_sweat * g_mask.b;
        N = normalize(N + (vec3(fract(q.x) - 0.5, fract(q.y) - 0.5, 0.0)) * bead * 0.6);
    }

    vec3 V = normalize(u_camPos - v_wpos);
    float metal = u_metallic, rough = u_roughness, ao = 1.0;
    if (u_hasMR == 1) { vec4 mr = texture(u_texMR, uvSel(u_uvMR)); rough *= mr.g; metal *= mr.b; }
    if (u_hasOcc == 1 && u_baked == 0) ao = mix(1.0, texture(u_texOcc, uvSel(u_uvOcc)).r, u_occStrength);
    if (u_mode == 1) {
        rough = max(rough, 0.44);
        rough = clamp(rough * (1.0 - 0.38 * u_sweat * g_mask.b), 0.26, 1.0);
        base.rgb *= mix(vec3(1.0), vec3(1.0, 0.84, 0.83), sat(u_flush) * 0.6 * g_mask.r);
    }
    if (u_mode == 3) rough = min(rough, 0.08);

    Surf s;
    s.strand = 1.0;
    if (u_mode == 2) {
        // hair cards: shade with normals bent toward the skull-centred sphere so the mass reads as one
        // volume with a coherent highlight band instead of per-card noise; strands run along +V (Bg)
        if (u_hairShape == 1) N = normalize(normalize(v_wpos - u_hairCenter) * 0.7 + N * 0.3);
        Tg = normalize(Bg - N * dot(N, Bg));
        s.strand = mix(0.7, 1.2, smoothstep(0.04, 0.55, dot(texS.rgb, vec3(0.3333))));
    }
    s.N = N; s.V = V; s.T = Tg; s.B = Bg;
    s.albedo = base.rgb; s.metal = metal; s.rough = clamp(rough, 0.03, 1.0); s.ao = ao; s.alpha = alpha; s.mode = u_mode;
    s.f0 = mix(vec3(u_mode == 1 ? 0.028 : 0.04), base.rgb, metal);

    float ssao = u_ssaoOn == 1 ? texture(u_ssao, gl_FragCoord.xy / u_screenSize).r : 1.0;
    vec3 emis = u_emissive;
    if (u_hasEmis == 1) emis *= texture(u_texEmis, uvSel(u_uvEmis)).rgb;

    // key spot light (bedside lamp) with soft shadows
    vec3 Lk = u_keyPos - v_wpos;
    float dk = length(Lk); Lk /= max(dk, 1e-4);
    float spot = smoothstep(u_keyCosOuter, u_keyCosInner, dot(-Lk, normalize(u_keyDir)));
    float fall = sat(1.0 - pow(dk / max(u_keyRange, 0.01), 4.0));
    float keyAtt = spot * fall * fall / (dk * dk + 0.25);
    float sh = keyAtt > 0.0 ? shadowFactor(v_wpos, Ng, Lk) : 1.0;

    if (u_mode == 5 || (u_mode == 4 && u_baked == 1)) {
        // baked lighting carried by the material: unlit base, or base x lightmap(emissive uv1)
        vec3 lit = (u_mode == 5) ? base.rgb : base.rgb * emis * u_bakeScale;
        float keyShare = sat(dot(Ng, Lk)) * keyAtt * u_keyIntensity * 0.35;
        float darken = 1.0 - 0.6 * (1.0 - sh) * sat(keyShare);
        vec3 c = lit * darken * mix(1.0, ssao, 0.7);
        o_color = vec4(c, u_pass == 1 ? sat(alpha / max(u_cutoff, 1e-3)) : 1.0);
        return;
    }

    vec3 col = vec3(0.0);
    col += shade(s, Lk, u_keyColor * u_keyIntensity * keyAtt * sh);
    col += shade(s, normalize(-u_moonDir), u_moonColor * u_moonIntensity);
    col += shade(s, normalize(-u_fillDir), u_fillColor * u_fillIntensity);
    for (int i = 0; i < u_numPoints; ++i) {
        vec3 Lp = u_pointPos[i] - v_wpos;
        float d = length(Lp); Lp /= max(d, 1e-4);
        float f = sat(1.0 - pow(d / max(u_pointRange[i], 0.01), 4.0));
        col += shade(s, Lp, u_pointColor[i] * u_pointIntensity[i] * f * f / (d * d + 0.05));
    }

    col *= mix(1.0, ssao, 0.3);   // contact darkening on direct light (subtle)
    ao *= ssao;
    // ambient: hemisphere irradiance + cheap glossy environment
    vec3 irr = mix(u_groundColor, u_skyColor, N.y * 0.5 + 0.5) * u_ambient;
    vec3 R = reflect(-V, N);
    vec3 env = mix(u_groundColor, u_skyColor, sat(R.y * 0.5 + 0.5)) * u_ambient;
    float NoV = max(dot(N, V), 1e-4);
    vec3 Fa = s.f0 + (max(vec3(1.0 - s.rough), s.f0) - s.f0) * pow(1.0 - NoV, 5.0);
    float specOcc = sat(pow(NoV + ao, exp2(-16.0 * s.rough - 1.0)) - 1.0 + ao);
    vec3 diffAmb = s.albedo * (1.0 - metal) * irr * ao;
    if (u_mode == 1) diffAmb *= vec3(1.0, 0.94, 0.92);
    vec3 specAmb = env * Fa * specOcc * (1.0 - s.rough * 0.7) * (u_mode == 3 ? 3.0 : 1.0);
    if (u_mode == 2) specAmb *= 0.12;
    col += diffAmb + specAmb + emis;

    if (u_mode == 3) {
        // wet cornea glint from the key light
        vec3 Hk = normalize(Lk + V);
        col += u_keyColor * u_keyIntensity * keyAtt * sh * pow(sat(dot(N, Hk)), 900.0) * 3.0;
    }
    float outA = 1.0;
    if (u_pass == 1) outA = sat(alpha / max(u_cutoff, 1e-3));
    else if (u_alphaMode == 2) outA = alpha;
    o_color = vec4(max(col, vec3(0.0)), outA);
}
