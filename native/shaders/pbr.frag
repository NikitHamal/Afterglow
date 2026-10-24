#version 330 core
// Forward PBR shading for Afterglow. Modes: 0 generic, 1 skin, 2 hair, 3 eye, 4 room, 5 unlit, 6 cloth.
// Output: linear HDR radiance (tonemapped later).
in vec3 v_wpos;
in vec3 v_nrm;
in vec4 v_tan;
in vec2 v_uv0;
in vec2 v_uv1;
in vec3 v_opos;                   // bind-space position / normal: garment patterns + the undress wipe
in vec3 v_onrm;
in float v_colA;                  // vertex colour alpha (COLOR_0): a hair shell's soft hairline
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
uniform int u_hairCards;          // hair: 1 cards (uv in [0,1], V root -> tip, 8 atlas columns), 0 a tiled shell / cap
uniform float u_flush;
uniform float u_sweat;
uniform int u_baked;              // room: emissive(uv1) / unlit carries baked light
uniform vec3 u_bakeScale;         // lightmap scale x lighting mood
uniform float u_bakeMix;          // 1 = baked room light, 0 = live lights (daylight relights the night bake)
uniform float u_bakeSat;          // saturation of the baked room (tames strongly coloured bedding)
uniform sampler2D u_skinMask;     // content skin support map (linear): R flush, G translucency, B sweat, A wetness
uniform int u_hasSkinMask;
uniform float u_wet;              // arousal wetness (her, where the mask's alpha says)
uniform float u_dissolve;         // garments: 1 worn .. 0 taken off
uniform vec4 u_wipe;              // garments: x,y bind-space height span, z 1 = comes off from the top
uniform int u_fabric;             // garments: 0 cotton 1 satin 2 lace 3 knit 4 denim 5 flannel 6 linen 7 nylon 8 wool
uniform vec3 u_fabric2;           // garments: pattern colour (plaid lines, lace)
uniform sampler2D u_detailN;      // tileable pore/crosshatch normal (uv0 * 48)
uniform int u_hasDetailN;
vec4 g_mask = vec4(0.6, 0.4, 0.5, 0.0);
uniform vec3 u_camPos;
uniform float u_time;
uniform vec3 u_headUp;            // eyes: the head's frame (the lids shade the eyeball relative to the head, not the eye)
uniform vec3 u_headFwd;

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
float h3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(h3(i), h3(i + vec3(1, 0, 0)), f.x), mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y);
    float b = mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x), mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y);
    return mix(a, b, f.z);
}
// garment fit: skin under a worn garment is never drawn (x: the outer layer is worn, y: the under layer is);
// a garment trims along its smoothed border (x: trim in m) and hides under the worn outer layer (y)
in vec2 v_fit;
uniform int u_fitMode;
uniform vec2 u_fitHide;
bool fitHidden() {
    if (u_fitMode == 1) return v_fit.x * u_fitHide.x > 0.5 || v_fit.y * u_fitHide.y > 0.5;
    if (u_fitMode == 2) return v_fit.x < u_fitHide.x || v_fit.y * u_fitHide.y > 0.5;
    return false;
}
// undressing: the garment wipes away along its bind-space height with a ragged, fabric-like edge (< 0: gone)
float undressMargin(vec3 op) {
    if (u_dissolve >= 0.999) return 1.0;
    float h = clamp((op.y - u_wipe.x) / max(u_wipe.y - u_wipe.x, 1e-3), 0.0, 1.0);
    float s = u_wipe.z > 0.5 ? 1.0 - h : h;
    float n = vnoise3(op * 38.0) * 0.6 + vnoise3(op * 110.0) * 0.4;
    float e = 0.16;
    return s - ((1.0 - u_dissolve) * (1.0 + e) - e * n);
}
// bump from a scalar height field without tangents (screen-space derivatives)
vec3 bumpNormal(vec3 N, float h, float amount) {
    vec3 dpdx = dFdx(v_wpos), dpdy = dFdy(v_wpos);
    float dhdx = dFdx(h), dhdy = dFdy(h);
    vec3 r1 = cross(dpdy, N), r2 = cross(N, dpdx);
    float det = dot(dpdx, r1);
    vec3 grad = sign(det) * (dhdx * r1 + dhdy * r2);
    return normalize(abs(det) * N - amount * grad);
}
// fabric weave / pattern in bind-space metres, projected on the three planes (garments are skin-tight)
vec3 g_tri = vec3(0.0);
// cloth (mode 6), set per fabric in main(): sheen colour + roughness (the fibres' fuzz), weight of the specular lobe,
// satin anisotropy (roughness along / across the floats, 0 = isotropic), wrapped diffuse
vec3 g_sheen = vec3(0.0);
float g_strandId = 0.5;           // hair: per-strand random (highlight shift / strength)
float g_sheenR = 0.5, g_specW = 1.0, g_aT = 0.0, g_aB = 0.0, g_wrap = 0.0;
float triPattern(int kind, float f) {
    vec3 p = v_opos * f;
    vec2 q[3] = vec2[](p.zy, p.xz, p.xy);
    float r = 0.0;
    for (int i = 0; i < 3; ++i) {
        vec2 c = q[i];
        float v;
        if (kind == 0) v = sin(c.x * 6.2831) * sin(c.y * 6.2831);                    // plain weave
        else if (kind == 1) v = abs(sin(c.x * 3.1416));                               // ribs (knit)
        else if (kind == 2) v = sin((c.x + c.y) * 6.2831);                            // twill (denim, wool)
        else v = sin(c.x * 6.2831 + 1.7 * sin(c.y * 0.9)) * sin(c.y * 6.2831);       // slubby (linen)
        r += v * g_tri[i];
    }
    return r;
}
float triPlaid(float f) {
    vec3 p = v_opos * f;
    vec2 q[3] = vec2[](p.zy, p.xz, p.xy);
    float r = 0.0;
    for (int i = 0; i < 3; ++i) {
        vec2 c = abs(fract(q[i]) - 0.5);
        float wide = max(smoothstep(0.36, 0.42, c.x), smoothstep(0.36, 0.42, c.y)) * 0.75;
        float thin = max(smoothstep(0.205, 0.215, c.x) * (1.0 - smoothstep(0.225, 0.235, c.x)),
                         smoothstep(0.205, 0.215, c.y) * (1.0 - smoothstep(0.225, 0.235, c.y)));
        r += max(wide, thin * 0.55) * g_tri[i];
    }
    return r;
}

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
    // 4 spread taps first: where they agree (fully lit / fully shadowed, most pixels) the other 12 add nothing
    float sum = 0.0;
    for (int i = 0; i < 4; ++i) {
        float d = texture(u_shadowMap, c.xy + R * POISSON[i] * radius).r;
        sum += (c.z - bias <= d) ? 1.0 : 0.0;
    }
    if (sum == 0.0 || sum == 4.0) return sum * 0.25;
    for (int i = 4; i < 16; ++i) {
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
        // every strand its own shift and strength, so highlights break into strands instead of one painted sheen;
        // a shell's flow radiates from the crown, so its narrow R lobe would draw a bright ring there: kept weak
        float shiftN = (g_strandId - 0.5) * 0.2;
        vec3 T1 = normalize(s.T + s.N * (-0.07 + shiftN));
        vec3 T2 = normalize(s.T + s.N * (0.1 + shiftN));
        float th1 = dot(T1, H), th2 = dot(T2, H);
        float sin1 = sqrt(max(0.0, 1.0 - th1 * th1)), sin2 = sqrt(max(0.0, 1.0 - th2 * th2));
        float dirAtten = smoothstep(-1.0, 0.0, th1);
        vec3 trt = sqrt(max(s.albedo, vec3(1e-4)));
        float rK = (u_hairCards == 1 ? 0.055 : 0.025) * (0.35 + 1.3 * g_strandId) * s.strand;
        vec3 spec = vec3(rK) * pow(sin1, 220.0) * dirAtten + trt * 0.12 * pow(sin2, 34.0) * s.strand * (0.55 + 0.9 * g_strandId);
        float wrap = sat((NoLraw + 0.4) / 1.4);
        vec3 diff = s.albedo / PI * wrap;
        return (diff + spec * sat(NoLraw * 0.7 + 0.3)) * radiance;
    }
    float a = max(s.rough * s.rough, 0.002);
    vec3 F = F_Schlick(VoH, s.f0);
    if (s.mode == 6) {
        // cloth: diffuse wrapped a little (light scatters through the fibres), a Charlie sheen (Estevez-Kulla) for the
        // fuzz with Neubelt-Pettineo visibility, and a weaker specular lobe: GGX for woven fabrics, anisotropic along
        // the floats for satin (a soft streak, not a plastic hotspot)
        float NoLw = sat((NoLraw + g_wrap) / (1.0 + g_wrap));
        float ir = 1.0 / max(g_sheenR, 0.07);
        float Dc = (2.0 + ir) * pow(max(1.0 - NoH * NoH, 0.0078125), ir * 0.5) / (2.0 * PI);
        vec3 sheen = g_sheen * Dc / (4.0 * (NoL + NoV - NoL * NoV) + 1e-4);
        vec3 specC = vec3(0.0);
        if (g_specW > 0.0) {
            if (g_aT > 0.0) {
                vec3 Tf = vec3(0.0, 1.0, 0.0) - s.N * s.N.y;   // floats run with the hang of the cloth (world up)
                Tf = dot(Tf, Tf) > 1e-6 ? normalize(Tf) : normalize(cross(s.N, vec3(1.0, 0.0, 0.0)));
                vec3 Bf = cross(s.N, Tf);
                float ToH = dot(Tf, H), BoH = dot(Bf, H);
                float d = ToH * ToH / (g_aT * g_aT) + BoH * BoH / (g_aB * g_aB) + NoH * NoH;
                float Da = 1.0 / (PI * g_aT * g_aB * d * d + 1e-7);
                float lv = NoL * length(vec3(g_aT * dot(Tf, s.V), g_aB * dot(Bf, s.V), NoV));
                float ll = NoV * length(vec3(g_aT * dot(Tf, L), g_aB * dot(Bf, L), NoL));
                specC = Da * (0.5 / max(lv + ll, 1e-5)) * F;
            } else {
                specC = D_GGX(NoH, a) * V_Smith(NoV, NoL, a) * F;
            }
            specC *= g_specW;
        }
        float sheenMax = max(g_sheen.r, max(g_sheen.g, g_sheen.b));
        vec3 diffC = s.albedo / PI * NoLw * (1.0 - F * g_specW) * (1.0 - 0.25 * sheenMax);
        return (diffC + (specC + sheen) * NoL) * radiance;
    }
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
    if (fitHidden()) discard;
    vec4 texS = u_hasBase == 1 ? texture(u_texBase, uvSel(u_uvBase)) : vec4(1.0);
    vec4 base = u_baseColor * texS;
    base.rgb *= u_tint;
    if (u_mode == 2) base.a *= v_colA;
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
    if (u_mode == 1 && u_hasSkinMask == 1) g_mask = texture(u_skinMask, v_uv0);
    float undressRim = 0.0;
    if (u_mode == 6) {
        float m = undressMargin(v_opos);
        if (m < 0.0) discard;
        undressRim = u_dissolve < 0.999 ? 1.0 - smoothstep(0.0, 0.035, m) : 0.0;
        vec3 w = pow(abs(normalize(v_onrm)), vec3(4.0));
        g_tri = w / max(w.x + w.y + w.z, 1e-4);
    }
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

    float fabricRough = -1.0;
    float insideK = 1.0;   // the inside of a garment: in its own shadow
    if (u_mode == 6) {
        // weave and pattern; the micro bump fades out before it would shimmer (period under ~2 px)
        float fpx = length(fwidth(v_opos));
        float k = 0.0, amt = 0.0, freq = 1.0;
        float sheenAmt = 0.35, hemW = 0.015, foldAmt = 0.003;
        g_sheenR = 0.5; g_specW = 0.25; g_wrap = 0.2;
        if (u_fabric == 0) { freq = 170.0; k = triPattern(0, freq); amt = 0.0012; fabricRough = 0.85; }
        else if (u_fabric == 1) {
            // satin: long floats, a soft anisotropic streak along the hang of the dress
            fabricRough = 0.45; sheenAmt = 0.25; g_sheenR = 0.35; g_specW = 0.7; g_aT = 0.14; g_aB = 0.5; g_wrap = 0.1;
            hemW = 0.006; foldAmt = 0.005;
        }
        else if (u_fabric == 2) {
            // lace: a floral-ish net over the skin; holes only while the cells are a few pixels wide
            freq = 140.0;
            vec3 p = v_opos * freq;
            vec2 q[3] = vec2[](p.zy, p.xz, p.xy);
            float hole = 0.0;
            for (int i = 0; i < 3; ++i) {
                vec2 c = fract(q[i]) - 0.5;
                float d = length(c), petals = 0.27 + 0.06 * cos(atan(c.y, c.x) * 5.0);
                hole += (smoothstep(petals - 0.03, petals, d) * (1.0 - smoothstep(0.42, 0.47, d))) * g_tri[i];
            }
            float cellPx = (1.0 / freq) / max(fpx, 1e-6);
            if (hole > 0.5 && cellPx > 5.0) discard;
            base.rgb = mix(base.rgb, u_fabric2, 0.5 * (1.0 - hole));
            fabricRough = 0.55; sheenAmt = 0.25; g_sheenR = 0.4; g_specW = 0.35; hemW = 0.0; foldAmt = 0.0015;
        }
        else if (u_fabric == 3) {
            freq = 260.0; k = triPattern(1, freq) * (0.75 + 0.25 * sin(v_opos.y * 1900.0)); amt = 0.0022; fabricRough = 0.92;
            sheenAmt = 0.7; g_sheenR = 0.65; g_specW = 0.0; g_wrap = 0.35; hemW = 0.03;   // knit: fuzzy, ribbed cuffs
        }
        else if (u_fabric == 4) {
            freq = 300.0; k = triPattern(2, freq); amt = 0.0012; fabricRough = 0.8;
            sheenAmt = 0.3; g_sheenR = 0.55; g_specW = 0.2; g_wrap = 0.15; hemW = 0.016; foldAmt = 0.002;
            base.rgb *= 0.86 + 0.28 * vnoise3(v_opos * 45.0) * vnoise3(v_opos * 9.0 + 3.1);   // worn indigo
        }
        else if (u_fabric == 5) {
            base.rgb = mix(base.rgb, u_fabric2, clamp(triPlaid(16.0), 0.0, 1.0));
            freq = 170.0; k = triPattern(0, freq); amt = 0.001; fabricRough = 0.86;
            sheenAmt = 0.6; g_sheenR = 0.7; g_specW = 0.05; g_wrap = 0.3; foldAmt = 0.0035;   // brushed flannel
        }
        else if (u_fabric == 6) {
            freq = 120.0; k = triPattern(3, freq); amt = 0.0016; fabricRough = 0.8;
            sheenAmt = 0.35; g_sheenR = 0.45; foldAmt = 0.004;
            base.rgb *= 0.93 + 0.1 * vnoise3(v_opos * 60.0);
        }
        else if (u_fabric == 7) { fabricRough = 0.38; sheenAmt = 0.15; g_sheenR = 0.35; g_specW = 0.6; g_wrap = 0.15; hemW = 0.008; foldAmt = 0.001; }
        else { freq = 420.0; k = triPattern(2, freq); amt = 0.0007; fabricRough = 0.74; sheenAmt = 0.45; g_sheenR = 0.55; g_specW = 0.15; hemW = 0.02; foldAmt = 0.0025; }
        // no woven fabric is whiter than ~0.72 albedo (fresh white cotton): keeps white shirts from glowing
        base.rgb *= min(1.0, 0.72 / max(max(base.r, base.g), max(base.b, 1e-4)));
        g_sheen = sheenAmt * mix(sqrt(max(base.rgb, vec3(1e-4))), vec3(1.0), 0.15);
        // soft folds: cloth never lies perfectly smooth (long folds where it hangs, finer ones where it is tight)
        if (foldAmt > 0.0) {
            float folds = vnoise3(v_opos * vec3(11.0, 3.0, 11.0)) * 0.7 + vnoise3(v_opos * vec3(26.0, 8.0, 26.0) + 4.1) * 0.3;
            N = bumpNormal(N, folds, foldAmt);
        }
        float fade = 1.0 - smoothstep(0.25, 0.6, fpx * freq);
        if (amt > 0.0 && fade > 0.0) N = bumpNormal(N, k * fade, amt * 0.35);   // amt: weave height (m)
        // the hem, outside: a rolled edge with the fabric's thickness, the doubled band ending in a small step and its
        // stitch line (only where a stitch is wider than a pixel); inside: shadowed, a little lighter near the opening
        if (u_fitMode == 2) {
            float hemD = v_fit.x - u_fitHide.x;   // metres from the trimmed edge
            if (!gl_FrontFacing) insideK = mix(0.8, 0.22, smoothstep(0.0, 0.06, hemD));
            else if (hemW > 0.0 && hemD < hemW + 0.004) {
                float px = length(fwidth(v_wpos)), st = (hemD - (hemW - 0.0035)) / 0.0006;
                float hh = 0.0012 * smoothstep(0.0, 0.003, hemD)
                         + 0.0004 * (1.0 - smoothstep(hemW - 0.001, hemW + 0.0008, hemD))
                         - 0.00035 * exp(-st * st) * (1.0 - smoothstep(0.0006, 0.0015, px));
                N = bumpNormal(N, hh, 1.0);
                base.rgb *= 1.0 - 0.16 * (1.0 - smoothstep(0.0, 0.0015, hemD));
            }
        }
        base.rgb *= 1.0 - 0.45 * undressRim;   // the edge where it lifts off reads as a shadowed fold
    }

    vec3 V = normalize(u_camPos - v_wpos);
    float metal = u_metallic, rough = u_roughness, ao = 1.0;
    if (u_hasMR == 1) { vec4 mr = texture(u_texMR, uvSel(u_uvMR)); rough *= mr.g; metal *= mr.b; }
    if (u_hasOcc == 1 && u_baked == 0) ao = mix(1.0, texture(u_texOcc, uvSel(u_uvOcc)).r, u_occStrength);
    if (fabricRough > 0.0) { rough = fabricRough; metal = 0.0; }
    if (u_mode == 1) {
        rough = max(rough, 0.44);
        rough = clamp(rough * (1.0 - 0.38 * u_sweat * g_mask.b) * (1.0 - 0.6 * g_mask.a * u_wet), 0.2, 1.0);
        base.rgb *= mix(vec3(1.0), vec3(1.0, 0.84, 0.83), sat(u_flush) * 0.6 * g_mask.r);
    }
    float eyeOcc = 1.0;
    if (u_mode == 3) {
        rough = min(rough, 0.08);
        // the upper lid and its lashes shade the top of the eyeball, the lower lid a little, the corners sit deep in
        // the socket: without this the whole ball is lit like a marble and the eyes read white and blank
        float up = dot(N, u_headUp);
        vec3 side = cross(u_headUp, u_headFwd);
        float lat = abs(dot(N, side)) / max(length(side), 1e-4);
        eyeOcc = mix(0.3, 1.0, smoothstep(0.40, 0.08, up)) * mix(0.82, 1.0, smoothstep(-0.48, -0.22, up))
               * mix(1.0, 0.62, smoothstep(0.35, 0.8, lat));
    }

    Surf s;
    s.strand = 1.0;
    if (u_mode == 2) {
        // hair cards: shade with normals bent toward the skull-centred sphere so the mass reads as one
        // volume with a coherent highlight band instead of per-card noise; strands run along +V (Bg)
        if (u_hairShape == 1) N = normalize(normalize(v_wpos - u_hairCenter) * 0.5 + N * 0.5);
        Tg = normalize(Bg - N * dot(N, Bg));
        g_strandId = hash12(vec2(floor(v_uv0.x * (u_hairCards == 1 ? 640.0 : 96.0)), 3.7));
        if (u_hairCards == 1) {
            // a card is a flat ribbon of hair: rounded across its width (U) so its highlight is a band, not a sheet,
            // and its root sits in the shade of the hair above it
            float x = fract(v_uv0.x * 8.0) * 2.0 - 1.0;
            vec3 across = normalize(cross(Tg, N));
            N = normalize(N * sqrt(max(0.05, 1.0 - 0.5 * x * x)) + across * x * 0.7);
            // no lock is ruler-straight: a little wave along each strand breaks one long sheen into glints
            float wave = sin(v_uv0.y * 46.0 + g_strandId * 6.2831) * 0.22 + sin(v_uv0.y * 17.0 + g_strandId * 12.57) * 0.12;
            Tg = normalize(Tg + across * wave);
            Tg = normalize(Tg - N * dot(N, Tg));
            base.rgb *= mix(0.5, 1.0, smoothstep(0.0, 0.22, v_uv0.y));
        }
        // highlights live on the strands, not in the gaps between them (coverage = the atlas alpha)
        s.strand = mix(0.7, 1.2, smoothstep(0.04, 0.55, dot(texS.rgb, vec3(0.3333)))) * mix(0.35, 1.0, smoothstep(0.2, 0.8, texS.a));
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

    vec3 bakedCol = vec3(0.0);
    bool bakedRoom = u_mode == 4 && u_baked == 1;
    if (u_mode == 5 || bakedRoom) {
        // baked lighting carried by the material: unlit base, or base x lightmap(emissive uv1)
        vec3 lit = (u_mode == 5) ? base.rgb : base.rgb * emis * u_bakeScale;
        if (bakedRoom) lit = mix(vec3(dot(lit, vec3(0.2126, 0.7152, 0.0722))), lit, u_bakeSat);
        float keyShare = sat(dot(Ng, Lk)) * keyAtt * u_keyIntensity * 0.35;
        float darken = 1.0 - 0.6 * (1.0 - sh) * sat(keyShare);
        bakedCol = lit * darken * mix(1.0, ssao, 0.7);
        if (u_mode == 5 || u_bakeMix >= 0.999) {
            o_color = vec4(bakedCol, u_pass == 1 ? sat(alpha / max(u_cutoff, 1e-3)) : 1.0);
            return;
        }
        emis = vec3(0.0);   // live path below: the lightmap is not emission
        s.albedo = mix(vec3(dot(s.albedo, vec3(0.2126, 0.7152, 0.0722))), s.albedo, u_bakeSat);
    }

    vec3 col = vec3(0.0);
    col += shade(s, Lk, u_keyColor * u_keyIntensity * keyAtt * sh);
    col += shade(s, normalize(-u_moonDir), u_moonColor * u_moonIntensity);
    col += shade(s, normalize(-u_fillDir), u_fillColor * u_fillIntensity);
    for (int i = 0; i < u_numPoints; ++i) {
        vec3 Lp = u_pointPos[i] - v_wpos;
        float d = length(Lp);
        if (d >= u_pointRange[i] || u_pointIntensity[i] <= 0.0) continue;   // out of reach: no shading work
        Lp /= max(d, 1e-4);
        float f = sat(1.0 - pow(d / max(u_pointRange[i], 0.01), 4.0));
        col += shade(s, Lp, u_pointColor[i] * u_pointIntensity[i] * f * f / (d * d + 0.05));
    }

    col *= mix(1.0, ssao, 0.3);   // contact darkening on direct light (subtle)
    col *= eyeOcc;
    ao *= ssao * eyeOcc;
    // ambient: hemisphere irradiance + cheap glossy environment
    vec3 irr = mix(u_groundColor, u_skyColor, N.y * 0.5 + 0.5) * u_ambient;
    vec3 R = reflect(-V, N);
    vec3 env = mix(u_groundColor, u_skyColor, sat(R.y * 0.5 + 0.5)) * u_ambient;
    float NoV = max(dot(N, V), 1e-4);
    vec3 Fa = s.f0 + (max(vec3(1.0 - s.rough), s.f0) - s.f0) * pow(1.0 - NoV, 5.0);
    float specOcc = sat(pow(NoV + ao, exp2(-16.0 * s.rough - 1.0)) - 1.0 + ao);
    vec3 diffAmb = s.albedo * (1.0 - metal) * irr * ao;
    if (u_mode == 1) diffAmb *= vec3(1.0, 0.94, 0.92);
    // the eye's tear film mirrors the room only where the lids leave it open (not a milky veil over the iris)
    vec3 specAmb = env * Fa * specOcc * (1.0 - s.rough * 0.7) * (u_mode == 3 ? eyeOcc * eyeOcc : 1.0);
    if (u_mode == 2) specAmb *= 0.12;
    if (u_mode == 6) specAmb = specAmb * g_specW * (g_aT > 0.0 ? 0.6 : 1.0) + g_sheen * irr * ao * (0.12 + 0.35 * pow(1.0 - NoV, 4.0));
    col += diffAmb + specAmb + emis;
    col *= insideK;
    if (bakedRoom) col = mix(col, bakedCol, u_bakeMix);

    if (u_mode == 3) {
        // wet cornea glints: a crisp catchlight from the key, a softer one from the camera-side fill, none under the lid
        float open = smoothstep(0.45, 0.8, eyeOcc);
        vec3 Hk = normalize(Lk + V);
        col += u_keyColor * u_keyIntensity * keyAtt * sh * pow(sat(dot(N, Hk)), 900.0) * 3.0 * open;
        vec3 Hf = normalize(normalize(-u_fillDir) + V);
        col += u_fillColor * u_fillIntensity * pow(sat(dot(N, Hf)), 600.0) * 1.2 * open;
    }
    float outA = 1.0;
    if (u_pass == 1) outA = sat(alpha / max(u_cutoff, 1e-3));
    else if (u_alphaMode == 2) outA = alpha;
    o_color = vec4(max(col, vec3(0.0)), outA);
}
