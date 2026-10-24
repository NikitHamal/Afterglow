#include "gfx/model.h"
#include "gfx/texture.h"
#include "core/log.h"
#include "core/paths.h"
#include "external/cgltf.h"   // implementation is compiled inside raylib (rmodels.c)
#include <chrono>
#include <cstring>
#include <future>
#include <map>
#include <algorithm>
#include <cctype>

namespace ag {

namespace {

Json parseExtras(const cgltf_extras& ex) {
    if (ex.data && ex.data[0]) return Json::parse(ex.data);
    return Json();
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<float> readFloats(const cgltf_accessor* a, int comps) {
    std::vector<float> out;
    if (!a) return out;
    out.resize(a->count * (size_t)comps);
    cgltf_size n = cgltf_num_components(a->type);
    if ((int)n == comps) {
        cgltf_accessor_unpack_floats(a, out.data(), out.size());
    } else {
        std::vector<float> tmp(a->count * n);
        cgltf_accessor_unpack_floats(a, tmp.data(), tmp.size());
        for (size_t i = 0; i < a->count; ++i)
            for (int c = 0; c < comps; ++c) out[i * comps + c] = c < (int)n ? tmp[i * n + c] : (c == 3 ? 1.f : 0.f);
    }
    return out;
}

void smoothNormals(const std::vector<float>& pos, const std::vector<uint32_t>& idx, std::vector<float>& nrm) {
    nrm.assign(pos.size(), 0.f);
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        uint32_t a = idx[t], b = idx[t + 1], c = idx[t + 2];
        V3 pa{pos[a * 3], pos[a * 3 + 1], pos[a * 3 + 2]}, pb{pos[b * 3], pos[b * 3 + 1], pos[b * 3 + 2]}, pc{pos[c * 3], pos[c * 3 + 1], pos[c * 3 + 2]};
        V3 n = cross(pb - pa, pc - pa);
        for (uint32_t v : {a, b, c}) { nrm[v * 3] += n.x; nrm[v * 3 + 1] += n.y; nrm[v * 3 + 2] += n.z; }
    }
    for (size_t v = 0; v * 3 < nrm.size(); ++v) {
        V3 n = normalize(V3{nrm[v * 3], nrm[v * 3 + 1], nrm[v * 3 + 2]});
        if (length(n) < 0.5f) n = {0, 1, 0};
        nrm[v * 3] = n.x; nrm[v * 3 + 1] = n.y; nrm[v * 3 + 2] = n.z;
    }
}

Shading classify(const std::string& name, bool unlit) {
    std::string n = lower(name);
    if (unlit) return Shading::Unlit;
    if (n.find("hair") != std::string::npos) return Shading::Hair;
    if (n.find("lash") != std::string::npos || n.find("brow") != std::string::npos) return Shading::Generic;
    if (n.find("eye") != std::string::npos) return Shading::Eye;
    if (n.find("skin") != std::string::npos || n.find("body") != std::string::npos || n.find("genital") != std::string::npos)
        return Shading::Skin;
    return Shading::Generic;
}

bool castsShadow(const std::string& name) {
    std::string n = lower(name);
    for (const char* k : {"eye", "teeth", "tongue", "lash", "brow"})
        if (n.find(k) != std::string::npos) return false;
    return true;
}

}  // namespace

void computeTangents(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<float>& uv,
                     const std::vector<uint32_t>& idx, std::vector<float>& tan) {
    size_t nv = pos.size() / 3;
    std::vector<V3> t1(nv, V3{0, 0, 0}), t2(nv, V3{0, 0, 0});
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        uint32_t i0 = idx[t], i1 = idx[t + 1], i2 = idx[t + 2];
        V3 p0{pos[i0 * 3], pos[i0 * 3 + 1], pos[i0 * 3 + 2]}, p1{pos[i1 * 3], pos[i1 * 3 + 1], pos[i1 * 3 + 2]}, p2{pos[i2 * 3], pos[i2 * 3 + 1], pos[i2 * 3 + 2]};
        float u0 = uv[i0 * 2], v0 = uv[i0 * 2 + 1], u1 = uv[i1 * 2], v1 = uv[i1 * 2 + 1], u2 = uv[i2 * 2], v2 = uv[i2 * 2 + 1];
        V3 e1 = p1 - p0, e2 = p2 - p0;
        float du1 = u1 - u0, dv1 = v1 - v0, du2 = u2 - u0, dv2 = v2 - v0;
        float det = du1 * dv2 - du2 * dv1;
        if (std::fabs(det) < 1e-12f) continue;
        float r = 1.f / det;
        V3 sdir = (e1 * dv2 - e2 * dv1) * r;
        V3 tdir = (e2 * du1 - e1 * du2) * r;
        for (uint32_t v : {i0, i1, i2}) { t1[v] += sdir; t2[v] += tdir; }
    }
    tan.assign(nv * 4, 0.f);
    for (size_t v = 0; v < nv; ++v) {
        V3 n{nrm[v * 3], nrm[v * 3 + 1], nrm[v * 3 + 2]};
        V3 t = normalize(t1[v] - n * dot(n, t1[v]));
        if (length(t) < 0.5f) {   // degenerate UVs: any perpendicular
            t = normalize(cross(n, std::fabs(n.y) < 0.9f ? V3{0, 1, 0} : V3{1, 0, 0}));
        }
        float w = dot(cross(n, t), t2[v]) < 0.f ? -1.f : 1.f;
        tan[v * 4] = t.x; tan[v * 4 + 1] = t.y; tan[v * 4 + 2] = t.z; tan[v * 4 + 3] = w;
    }
}

std::function<void()> Model::loadPump;
static long long gBufBytes = 0;
long long Model::bufferBytes() { return gBufBytes; }

void Model::uploadPrimitive(Primitive& p, const std::vector<float>& uv0, const std::vector<float>& uv1) {
    glGenVertexArrays(1, &p.vao);
    glBindVertexArray(p.vao);
    auto buf = [&](int slot, const std::vector<float>& data, int comps, bool dyn) {
        if (data.empty()) {
            glDisableVertexAttribArray((GLuint)slot);
            return;
        }
        glGenBuffers(1, &p.vbo[slot]);
        glBindBuffer(GL_ARRAY_BUFFER, p.vbo[slot]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * sizeof(float)), data.data(), dyn ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        gBufBytes += (long long)(data.size() * sizeof(float));
        glVertexAttribPointer((GLuint)slot, comps, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray((GLuint)slot);
    };
    buf(0, p.pos, 3, p.dynamic);
    buf(1, p.nrm, 3, p.dynamic);
    buf(2, uv0, 2, false);
    buf(3, uv1, 2, false);
    buf(4, p.tan, 4, false);
    if (!p.joints.empty()) {
        std::vector<float> jf(p.joints.begin(), p.joints.end());
        buf(5, jf, 4, false);
        buf(6, p.weights, 4, false);
    }
    glGenBuffers(1, &p.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(p.idx.size() * sizeof(uint32_t)), p.idx.data(), GL_STATIC_DRAW);
    gBufBytes += (long long)(p.idx.size() * sizeof(uint32_t));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // generic defaults for missing attributes
    p.indexCount = (int)p.idx.size();
    p.vertexCount = (int)(p.pos.size() / 3);
    p.hasUv1 = !uv1.empty();
    if (!p.dynamic) { std::vector<float>().swap(p.tan); }
}

// dense per-target deltas -> only the vertices each target actually moves
static void packTargets(Primitive& p) {
    size_t nv = p.pos.size() / 3;
    p.targetCount = (int)p.dPos.size();
    p.targets.assign(p.dPos.size(), {});
    for (size_t t = 0; t < p.dPos.size(); ++t) {
        const std::vector<float>& dp = p.dPos[t];
        const std::vector<float>* dn = t < p.dNrm.size() && p.dNrm[t].size() == nv * 3 ? &p.dNrm[t] : nullptr;
        Primitive::Sparse& sp = p.targets[t];
        if (dp.size() != nv * 3) continue;
        for (size_t v = 0; v < nv; ++v) {
            float a = std::fabs(dp[v * 3]) + std::fabs(dp[v * 3 + 1]) + std::fabs(dp[v * 3 + 2]);
            float b = dn ? std::fabs((*dn)[v * 3]) + std::fabs((*dn)[v * 3 + 1]) + std::fabs((*dn)[v * 3 + 2]) : 0.f;
            if (a < 1e-7f && b < 1e-6f) continue;
            sp.idx.push_back((uint32_t)v);
            sp.dp.insert(sp.dp.end(), {dp[v * 3], dp[v * 3 + 1], dp[v * 3 + 2]});
            if (dn) sp.dn.insert(sp.dn.end(), {(*dn)[v * 3], (*dn)[v * 3 + 1], (*dn)[v * 3 + 2]});
        }
        sp.idx.shrink_to_fit(); sp.dp.shrink_to_fit(); sp.dn.shrink_to_fit();
    }
    std::vector<std::vector<float>>().swap(p.dPos);
    std::vector<std::vector<float>>().swap(p.dNrm);
}

void Model::updateMorph(Primitive& p, const float* w, int count) {
    if (!p.dynamic || !p.vbo[0]) return;
    if (p.scratchPos.size() != p.pos.size()) { p.scratchPos = p.pos; p.scratchNrm = p.nrm; p.dirtyLo = 0; p.dirtyHi = (uint32_t)(p.pos.size() / 3); }
    // the vertices touched now, plus last time's (they go back to base): only that range is rebuilt and uploaded
    uint32_t lo = p.dirtyLo, hi = p.dirtyHi, nlo = 0xffffffffu, nhi = 0;
    for (int t = 0; t < count && t < (int)p.targets.size(); ++t) {
        if (std::fabs(w[t]) < 1e-4f || p.targets[t].idx.empty()) continue;
        nlo = std::min(nlo, p.targets[t].idx.front());
        nhi = std::max(nhi, p.targets[t].idx.back() + 1);
    }
    if (nhi > 0) { lo = std::min(lo, nlo); hi = std::max(hi, nhi); }
    if (hi <= lo) { p.dirtyLo = p.dirtyHi = 0; return; }
    std::copy(p.pos.begin() + lo * 3, p.pos.begin() + hi * 3, p.scratchPos.begin() + lo * 3);
    std::copy(p.nrm.begin() + lo * 3, p.nrm.begin() + hi * 3, p.scratchNrm.begin() + lo * 3);
    p.dirtyLo = nhi > 0 ? nlo : 0;
    p.dirtyHi = nhi;
    bool anyN = false;
    for (int t = 0; t < count && t < (int)p.targets.size(); ++t) {
        float wt = w[t];
        if (std::fabs(wt) < 1e-4f) continue;
        const Primitive::Sparse& sp = p.targets[t];
        const size_t n = sp.idx.size();
        float* P = p.scratchPos.data();
        for (size_t k = 0; k < n; ++k) {
            float* d = P + (size_t)sp.idx[k] * 3;
            d[0] += wt * sp.dp[k * 3]; d[1] += wt * sp.dp[k * 3 + 1]; d[2] += wt * sp.dp[k * 3 + 2];
        }
        if (!sp.dn.empty()) {
            float* N = p.scratchNrm.data();
            for (size_t k = 0; k < n; ++k) {
                float* d = N + (size_t)sp.idx[k] * 3;
                d[0] += wt * sp.dn[k * 3]; d[1] += wt * sp.dn[k * 3 + 1]; d[2] += wt * sp.dn[k * 3 + 2];
            }
            anyN = true;
        }
    }
    if (anyN) {
        // renormalise only what the active targets touched (idempotent where they overlap)
        float* N = p.scratchNrm.data();
        for (int t = 0; t < count && t < (int)p.targets.size(); ++t) {
            if (std::fabs(w[t]) < 1e-4f || p.targets[t].dn.empty()) continue;
            for (uint32_t v : p.targets[t].idx) {
                float* n = N + (size_t)v * 3;
                float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (l > 1e-8f) { float k = 1.f / l; n[0] *= k; n[1] *= k; n[2] *= k; }
            }
        }
    }
    GLintptr off = (GLintptr)(lo * 3 * sizeof(float));
    GLsizeiptr bytes = (GLsizeiptr)((hi - lo) * 3 * sizeof(float));
    glBindBuffer(GL_ARRAY_BUFFER, p.vbo[0]);
    glBufferSubData(GL_ARRAY_BUFFER, off, bytes, p.scratchPos.data() + lo * 3);
    glBindBuffer(GL_ARRAY_BUFFER, p.vbo[1]);
    glBufferSubData(GL_ARRAY_BUFFER, off, bytes, p.scratchNrm.data() + lo * 3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Model::load(const std::string& p) {
    destroy();
    path = p;
    std::string bytes = paths::readText(p);
    if (bytes.empty()) { AG_ERROR("AG: cannot read %s", p.c_str()); return false; }
    cgltf_options opt;
    std::memset(&opt, 0, sizeof opt);
    cgltf_data* d = nullptr;
    if (cgltf_parse(&opt, bytes.data(), bytes.size(), &d) != cgltf_result_success) { AG_ERROR("AG: glTF parse failed %s", p.c_str()); return false; }
    if (cgltf_load_buffers(&opt, d, p.c_str()) != cgltf_result_success) { AG_ERROR("AG: glTF buffers failed %s", p.c_str()); cgltf_free(d); return false; }

    // ---- textures: embedded images decoded in parallel (CPU), uploaded lazily per colour space (GPU) ----
    std::vector<DecodedImage> decoded(d->images_count);
    {
        std::vector<std::future<void>> jobs;
        for (size_t i = 0; i < d->images_count; ++i) {
            const cgltf_image& im = d->images[i];
            if (!im.buffer_view) continue;
            const uint8_t* data = cgltf_buffer_view_data(im.buffer_view);
            int size = (int)im.buffer_view->size;
            if (!data || size <= 0) continue;
            DecodedImage* dst = &decoded[i];
            jobs.push_back(std::async(std::launch::async, [data, size, dst] { decodeImage(data, size, *dst); }));
        }
        for (auto& j : jobs) {
            while (j.wait_for(std::chrono::milliseconds(15)) != std::future_status::ready)
                if (loadPump) loadPump();
            j.get();
        }
    }
    std::map<std::pair<int, bool>, GLuint> texCache;
    auto texFor = [&](const cgltf_texture_view& v, bool srgb) -> GLuint {
        if (!v.texture || !v.texture->image) return 0;
        const cgltf_image* img = v.texture->image;
        auto key = std::make_pair((int)cgltf_image_index(d, img), srgb);
        auto it = texCache.find(key);
        if (it != texCache.end()) return it->second;
        GLuint t = 0;
        const DecodedImage& pre = decoded[(size_t)key.first];
        if (pre.w > 0) {
            t = textureFromDecoded(pre, srgb);
        } else if (img->buffer_view) {
            const uint8_t* data = cgltf_buffer_view_data(img->buffer_view);
            if (data) t = textureFromMemory(data, (int)img->buffer_view->size, srgb);
        } else if (img->uri && std::strncmp(img->uri, "data:", 5) != 0) {
            t = textureFromFile(paths::join(paths::dirname(p), img->uri), srgb);
        }
        if (t) {
            textures.push_back(t);
            if (v.texture->sampler) {
                glBindTexture(GL_TEXTURE_2D, t);
                if (v.texture->sampler->wrap_s) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, v.texture->sampler->wrap_s);
                if (v.texture->sampler->wrap_t) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, v.texture->sampler->wrap_t);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        texCache[key] = t;
        return t;
    };

    // ---- materials ----
    for (size_t i = 0; i < d->materials_count; ++i) {
        const cgltf_material& cm = d->materials[i];
        Material m;
        m.name = cm.name ? cm.name : ("material_" + std::to_string(i));
        if (cm.has_pbr_metallic_roughness) {
            const auto& pbr = cm.pbr_metallic_roughness;
            m.baseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2], pbr.base_color_factor[3]};
            m.texBase = texFor(pbr.base_color_texture, true);
            m.uvBase = pbr.base_color_texture.texcoord;
            m.metallic = pbr.metallic_factor;
            m.roughness = pbr.roughness_factor;
            m.texMR = texFor(pbr.metallic_roughness_texture, false);
            m.uvMR = pbr.metallic_roughness_texture.texcoord;
        }
        m.texNormal = texFor(cm.normal_texture, false);
        m.uvNormal = cm.normal_texture.texcoord;
        m.normalScale = cm.normal_texture.texture ? cm.normal_texture.scale : 1.f;
        m.texOcc = texFor(cm.occlusion_texture, false);
        m.uvOcc = cm.occlusion_texture.texcoord;
        m.occlusion = cm.occlusion_texture.texture ? cm.occlusion_texture.scale : 1.f;
        m.texEmis = texFor(cm.emissive_texture, true);
        m.uvEmis = cm.emissive_texture.texcoord;
        m.emissive = {cm.emissive_factor[0], cm.emissive_factor[1], cm.emissive_factor[2]};
        if (cm.has_emissive_strength) m.emissive = m.emissive * cm.emissive_strength.emissive_strength;
        m.alpha = cm.alpha_mode == cgltf_alpha_mode_mask ? AlphaMode::Mask : (cm.alpha_mode == cgltf_alpha_mode_blend ? AlphaMode::Blend : AlphaMode::Opaque);
        m.cutoff = cm.alpha_cutoff;
        m.doubleSided = cm.double_sided != 0;
        m.unlit = cm.unlit != 0;
        m.shading = classify(m.name, m.unlit);
        m.castShadow = castsShadow(m.name);
        m.extras = parseExtras(cm.extras);
        materials.push_back(std::move(m));
    }
    if (materials.empty()) { Material m; m.name = "default"; m.metallic = 0.f; m.roughness = 0.6f; materials.push_back(m); }

    // ---- nodes ----
    nodes.resize(d->nodes_count);
    for (size_t i = 0; i < d->nodes_count; ++i) {
        const cgltf_node& cn = d->nodes[i];
        Node& n = nodes[i];
        n.name = cn.name ? cn.name : ("node_" + std::to_string(i));
        if (cn.has_matrix) {
            M4 m; std::memcpy(m.m, cn.matrix, sizeof m.m);
            decomposeTRS(m, n.t, n.r, n.s);
        } else {
            if (cn.has_translation) n.t = {cn.translation[0], cn.translation[1], cn.translation[2]};
            if (cn.has_rotation) n.r = qnormalize({cn.rotation[0], cn.rotation[1], cn.rotation[2], cn.rotation[3]});
            if (cn.has_scale) n.s = {cn.scale[0], cn.scale[1], cn.scale[2]};
        }
        n.parent = cn.parent ? (int)cgltf_node_index(d, cn.parent) : -1;
        for (size_t c = 0; c < cn.children_count; ++c) n.children.push_back((int)cgltf_node_index(d, cn.children[c]));
        n.mesh = cn.mesh ? (int)cgltf_mesh_index(d, cn.mesh) : -1;
        n.skin = cn.skin ? (int)cgltf_skin_index(d, cn.skin) : -1;
        n.light = cn.light ? (int)cgltf_light_index(d, cn.light) : -1;
        n.extras = parseExtras(cn.extras);
        nodeIndex.emplace(n.name, (int)i);
    }
    const cgltf_scene* sc = d->scene ? d->scene : (d->scenes_count ? &d->scenes[0] : nullptr);
    if (sc) {
        for (size_t i = 0; i < sc->nodes_count; ++i) roots.push_back((int)cgltf_node_index(d, sc->nodes[i]));
        sceneExtras = parseExtras(sc->extras);
    } else {
        for (size_t i = 0; i < nodes.size(); ++i) if (nodes[i].parent < 0) roots.push_back((int)i);
    }

    // ---- meshes ----
    meshes.resize(d->meshes_count);
    for (size_t mi = 0; mi < d->meshes_count; ++mi) {
        const cgltf_mesh& cmsh = d->meshes[mi];
        Mesh& mesh = meshes[mi];
        mesh.name = cmsh.name ? cmsh.name : ("mesh_" + std::to_string(mi));
        for (size_t k = 0; k < cmsh.target_names_count; ++k) mesh.targetNames.push_back(cmsh.target_names[k] ? cmsh.target_names[k] : "");
        mesh.defaultWeights.assign(cmsh.weights, cmsh.weights + cmsh.weights_count);
        for (size_t pi = 0; pi < cmsh.primitives_count; ++pi) {
            const cgltf_primitive& cp = cmsh.primitives[pi];
            if (cp.type != cgltf_primitive_type_triangles) continue;
            Primitive prim;
            std::vector<float> uv0, uv1;
            std::vector<float> jointsF;
            for (size_t a = 0; a < cp.attributes_count; ++a) {
                const cgltf_attribute& at = cp.attributes[a];
                switch (at.type) {
                    case cgltf_attribute_type_position: prim.pos = readFloats(at.data, 3); break;
                    case cgltf_attribute_type_normal: prim.nrm = readFloats(at.data, 3); break;
                    case cgltf_attribute_type_tangent: prim.tan = readFloats(at.data, 4); break;
                    case cgltf_attribute_type_texcoord:
                        if (at.index == 0) uv0 = readFloats(at.data, 2);
                        else if (at.index == 1) uv1 = readFloats(at.data, 2);
                        break;
                    case cgltf_attribute_type_joints:
                        if (at.index == 0) {
                            prim.joints.resize(at.data->count * 4);
                            for (size_t v = 0; v < at.data->count; ++v) {
                                cgltf_uint j[4] = {0, 0, 0, 0};
                                cgltf_accessor_read_uint(at.data, v, j, 4);
                                for (int c = 0; c < 4; ++c) prim.joints[v * 4 + c] = (uint16_t)j[c];
                            }
                        }
                        break;
                    case cgltf_attribute_type_weights:
                        if (at.index == 0) prim.weights = readFloats(at.data, 4);
                        break;
                    default: break;
                }
            }
            if (prim.pos.empty()) continue;
            size_t nv = prim.pos.size() / 3;
            if (cp.indices) {
                prim.idx.resize(cp.indices->count);
                cgltf_accessor_unpack_indices(cp.indices, prim.idx.data(), sizeof(uint32_t), prim.idx.size());
            } else {
                prim.idx.resize(nv);
                for (size_t v = 0; v < nv; ++v) prim.idx[v] = (uint32_t)v;
            }
            if (prim.nrm.size() != nv * 3) smoothNormals(prim.pos, prim.idx, prim.nrm);
            if (prim.tan.size() != nv * 4) {
                if (uv0.size() == nv * 2) computeTangents(prim.pos, prim.nrm, uv0, prim.idx, prim.tan);
                else prim.tan.clear();
            }
            if (!prim.weights.empty()) {
                for (size_t v = 0; v < nv; ++v) {
                    float* w = &prim.weights[v * 4];
                    float s = w[0] + w[1] + w[2] + w[3];
                    if (s > 1e-6f) for (int c = 0; c < 4; ++c) w[c] /= s;
                    else { w[0] = 1.f; w[1] = w[2] = w[3] = 0.f; }
                }
            }
            prim.skinned = !prim.joints.empty() && !prim.weights.empty();
            if (!prim.skinned) { prim.joints.clear(); prim.weights.clear(); }
            // morph targets
            prim.dPos.resize(cp.targets_count);
            prim.dNrm.resize(cp.targets_count);
            bool anyMorph = false;
            for (size_t t = 0; t < cp.targets_count; ++t) {
                const cgltf_morph_target& mt = cp.targets[t];
                for (size_t a = 0; a < mt.attributes_count; ++a) {
                    const cgltf_attribute& at = mt.attributes[a];
                    if (at.type == cgltf_attribute_type_position) { prim.dPos[t] = readFloats(at.data, 3); anyMorph = true; }
                    else if (at.type == cgltf_attribute_type_normal) prim.dNrm[t] = readFloats(at.data, 3);
                }
                if (prim.dPos[t].size() != nv * 3) prim.dPos[t].clear();
                if (prim.dNrm[t].size() != nv * 3) prim.dNrm[t].clear();
            }
            prim.dynamic = anyMorph;
            prim.material = cp.material ? (int)cgltf_material_index(d, cp.material) : 0;
            prim.bmin = {1e9f, 1e9f, 1e9f}; prim.bmax = {-1e9f, -1e9f, -1e9f};
            for (size_t v = 0; v < nv; ++v) {
                prim.bmin.x = std::min(prim.bmin.x, prim.pos[v * 3]); prim.bmax.x = std::max(prim.bmax.x, prim.pos[v * 3]);
                prim.bmin.y = std::min(prim.bmin.y, prim.pos[v * 3 + 1]); prim.bmax.y = std::max(prim.bmax.y, prim.pos[v * 3 + 1]);
                prim.bmin.z = std::min(prim.bmin.z, prim.pos[v * 3 + 2]); prim.bmax.z = std::max(prim.bmax.z, prim.pos[v * 3 + 2]);
            }
            uploadPrimitive(prim, uv0, uv1);
            packTargets(prim);
            mesh.prims.push_back(std::move(prim));
        }
    }

    // ---- skins ----
    skins.resize(d->skins_count);
    for (size_t si = 0; si < d->skins_count; ++si) {
        const cgltf_skin& cs = d->skins[si];
        Skin& s = skins[si];
        for (size_t j = 0; j < cs.joints_count; ++j) s.joints.push_back((int)cgltf_node_index(d, cs.joints[j]));
        s.invBind.assign(cs.joints_count, M4::identity());
        if (cs.inverse_bind_matrices)
            for (size_t j = 0; j < cs.joints_count; ++j) cgltf_accessor_read_float(cs.inverse_bind_matrices, j, s.invBind[j].m, 16);
        maxJoints = std::max(maxJoints, (int)cs.joints_count);
    }

    // ---- skin guard: bad exporters occasionally leave a vertex pointing past
    // the skin or with a non-unit influence sum.  Rebind those influences to
    // the first joint instead of allowing an out-of-bounds bone fetch to fold
    // the whole mesh.  The authored file remains untouched; this is the
    // runtime copy consumed by the renderer.
    for (size_t ni = 0; ni < nodes.size(); ++ni) {
        const Node& node = nodes[ni];
        if (node.mesh < 0 || node.mesh >= (int)meshes.size() || node.skin < 0 || node.skin >= (int)skins.size()) continue;
        const Skin& skin = skins[(size_t)node.skin];
        for (Primitive& pr : meshes[(size_t)node.mesh].prims) {
            if (!pr.skinned || pr.joints.empty() || pr.weights.empty() || skin.joints.empty()) continue;
            size_t nv = std::min(pr.joints.size() / 4, pr.weights.size() / 4);
            int repaired = 0;
            for (size_t v = 0; v < nv; ++v) {
                float sum = 0.f;
                for (int k = 0; k < 4; ++k) {
                    uint16_t& j = pr.joints[v * 4 + (size_t)k];
                    float& w = pr.weights[v * 4 + (size_t)k];
                    if (j >= skin.joints.size() || !std::isfinite(w) || w < 0.f) { j = 0; w = 0.f; ++repaired; }
                    sum += w;
                }
                if (sum < 1e-6f) { pr.joints[v * 4] = 0; pr.weights[v * 4] = 1.f; pr.weights[v * 4 + 1] = pr.weights[v * 4 + 2] = pr.weights[v * 4 + 3] = 0.f; ++repaired; }
                else for (int k = 0; k < 4; ++k) pr.weights[v * 4 + (size_t)k] /= sum;
            }
            if (repaired) AG_WARN("AG: skin guard repaired %d influences in %s mesh %s", repaired, p.c_str(), meshes[(size_t)node.mesh].name.c_str());
        }
    }

    // ---- animations ----
    for (size_t ai = 0; ai < d->animations_count; ++ai) {
        const cgltf_animation& ca = d->animations[ai];
        Clip clip;
        clip.name = ca.name ? ca.name : ("clip_" + std::to_string(ai));
        for (size_t ci = 0; ci < ca.channels_count; ++ci) {
            const cgltf_animation_channel& cc = ca.channels[ci];
            if (!cc.target_node || !cc.sampler) continue;
            Channel ch;
            ch.node = (int)cgltf_node_index(d, cc.target_node);
            switch (cc.target_path) {
                case cgltf_animation_path_type_translation: ch.path = 0; ch.comps = 3; break;
                case cgltf_animation_path_type_rotation: ch.path = 1; ch.comps = 4; break;
                case cgltf_animation_path_type_scale: ch.path = 2; ch.comps = 3; break;
                case cgltf_animation_path_type_weights: {
                    ch.path = 3;
                    const cgltf_mesh* m = cc.target_node->mesh;
                    ch.comps = m ? (int)std::max<size_t>(m->target_names_count, m->primitives_count ? m->primitives[0].targets_count : 0) : 0;
                    break;
                }
                default: continue;
            }
            if (ch.comps <= 0) continue;
            const cgltf_animation_sampler* smp = cc.sampler;
            ch.interp = smp->interpolation == cgltf_interpolation_type_step ? 1 : (smp->interpolation == cgltf_interpolation_type_cubic_spline ? 2 : 0);
            ch.times = readFloats(smp->input, 1);
            size_t outN = smp->output->count * cgltf_num_components(smp->output->type);
            ch.values.resize(outN);
            cgltf_accessor_unpack_floats(smp->output, ch.values.data(), outN);
            if (ch.path == 3 && !ch.times.empty()) {
                size_t per = ch.values.size() / ch.times.size() / (ch.interp == 2 ? 3 : 1);
                ch.comps = (int)per;
            }
            if (!ch.times.empty()) clip.duration = std::max(clip.duration, ch.times.back());
            clip.channels.push_back(std::move(ch));
        }
        clips.push_back(std::move(clip));
    }

    // ---- lights (KHR_lights_punctual) ----
    for (size_t li = 0; li < d->lights_count; ++li) {
        const cgltf_light& cl = d->lights[li];
        Light l;
        l.name = cl.name ? cl.name : ("light_" + std::to_string(li));
        l.type = cl.type == cgltf_light_type_directional ? 0 : (cl.type == cgltf_light_type_spot ? 2 : 1);
        l.color = {cl.color[0], cl.color[1], cl.color[2]};
        l.intensity = cl.intensity;
        l.range = cl.range;
        l.inner = cl.spot_inner_cone_angle;
        l.outer = cl.spot_outer_cone_angle;
        lights.push_back(l);
    }
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].light >= 0 && nodes[i].light < (int)lights.size()) lights[nodes[i].light].node = (int)i;

    cgltf_free(d);
    size_t tris = 0;
    for (auto& m : meshes) for (auto& pr : m.prims) tris += pr.idx.size() / 3;
    AG_INFO("AG: loaded %s: %d nodes, %d meshes, %zu tris, %d materials, %d skins, %d clips, %d textures",
            p.c_str(), (int)nodes.size(), (int)meshes.size(), tris, (int)materials.size(), (int)skins.size(), (int)clips.size(), (int)textures.size());
    return true;
}

void Model::releaseCpuData() {
    for (auto& m : meshes)
        for (auto& p : m.prims) {
            std::vector<uint32_t>().swap(p.idx);
            std::vector<uint16_t>().swap(p.joints);
            std::vector<float>().swap(p.weights);
            std::vector<float>().swap(p.tan);
            if (!p.dynamic) { std::vector<float>().swap(p.pos); std::vector<float>().swap(p.nrm); }
            std::vector<float>().swap(p.scratchPos);
            std::vector<float>().swap(p.scratchNrm);
        }
}

void Model::destroy() {
    for (auto& m : meshes)
        for (auto& p : m.prims) {
            if (p.vao) glDeleteVertexArrays(1, &p.vao);
            for (GLuint& b : p.vbo) if (b) { glDeleteBuffers(1, &b); b = 0; }
            if (p.ebo) glDeleteBuffers(1, &p.ebo);
            p.vao = p.ebo = 0;
        }
    for (GLuint& t : textures) textureDestroy(t);
    textures.clear(); meshes.clear(); nodes.clear(); roots.clear(); materials.clear();
    skins.clear(); clips.clear(); lights.clear(); nodeIndex.clear();
    maxJoints = 0;
}

int Model::findNode(const std::string& name) const {
    auto it = nodeIndex.find(name);
    return it == nodeIndex.end() ? -1 : it->second;
}
int Model::findClip(const std::string& name) const {
    for (size_t i = 0; i < clips.size(); ++i) if (clips[i].name == name) return (int)i;
    return -1;
}
int Model::findMaterial(const std::string& name) const {
    for (size_t i = 0; i < materials.size(); ++i) if (materials[i].name == name) return (int)i;
    return -1;
}
void Model::localBounds(V3& mn, V3& mx) const {
    mn = {1e9f, 1e9f, 1e9f}; mx = {-1e9f, -1e9f, -1e9f};
    for (auto& m : meshes)
        for (auto& p : m.prims) {
            mn = {std::min(mn.x, p.bmin.x), std::min(mn.y, p.bmin.y), std::min(mn.z, p.bmin.z)};
            mx = {std::max(mx.x, p.bmax.x), std::max(mx.y, p.bmax.y), std::max(mx.z, p.bmax.z)};
        }
}

}  // namespace ag
