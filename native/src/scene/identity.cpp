#include "scene/identity.h"
#include "core/json.h"
#include "core/log.h"
#include "core/paths.h"
#include <cmath>
#include <cstring>

namespace ag {

namespace {
struct Entry { int mesh = -1, prim = 0, n = 0; size_t offset = 0; };
}

bool applyIdentityPack(Model& model, const std::string& file) {
    std::string raw = paths::readText(file);
    if (raw.size() < 12 || std::memcmp(raw.data(), "AGID", 4) != 0) { AG_WARN("AG: identity %s: not a pack", file.c_str()); return false; }
    uint32_t version = 0, jsonLen = 0;
    std::memcpy(&version, raw.data() + 4, 4);
    std::memcpy(&jsonLen, raw.data() + 8, 4);
    if (version != 1 || 12ull + jsonLen > raw.size()) { AG_WARN("AG: identity %s: bad header", file.c_str()); return false; }
    Json head = Json::parse(raw.substr(12, jsonLen));
    const char* blob = raw.data() + 12 + jsonLen;
    size_t blobSize = raw.size() - 12 - jsonLen;
    // validate everything first: a pack for another base must not half-apply
    std::vector<Entry> entries;
    for (const Json& e : head["meshes"].arr) {
        Entry en;
        std::string name = e["name"].str();
        for (size_t mi = 0; mi < model.meshes.size(); ++mi) if (model.meshes[mi].name == name) en.mesh = (int)mi;
        en.prim = (int)e["prim"].num(0);
        en.n = (int)e["n"].num(0);
        en.offset = (size_t)e["offset"].num(0);
        if (en.mesh < 0 || en.prim < 0 || en.prim >= (int)model.meshes[(size_t)en.mesh].prims.size()) {
            AG_WARN("AG: identity %s: no mesh %s[%d]", file.c_str(), name.c_str(), en.prim); return false;
        }
        const Primitive& p = model.meshes[(size_t)en.mesh].prims[(size_t)en.prim];
        if (p.vertexCount != (int)e["vertexCount"].num(-1) || p.pos.size() != (size_t)p.vertexCount * 3 || p.nrm.size() != p.pos.size()) {
            AG_WARN("AG: identity %s: %s[%d] has %d vertices (pack: %d)", file.c_str(), name.c_str(), en.prim, p.vertexCount,
                    (int)e["vertexCount"].num(-1));
            return false;
        }
        if (en.offset + (size_t)en.n * 28 > blobSize) { AG_WARN("AG: identity %s: truncated", file.c_str()); return false; }
        entries.push_back(en);
    }
    int moved = 0;
    for (const Entry& en : entries) {
        Primitive& p = model.meshes[(size_t)en.mesh].prims[(size_t)en.prim];
        const char* b = blob + en.offset;
        std::vector<uint32_t> idx((size_t)en.n);
        std::vector<float> dp((size_t)en.n * 3), dn((size_t)en.n * 3);
        std::memcpy(idx.data(), b, idx.size() * 4);
        std::memcpy(dp.data(), b + idx.size() * 4, dp.size() * 4);
        std::memcpy(dn.data(), b + idx.size() * 4 + dp.size() * 4, dn.size() * 4);
        for (size_t k = 0; k < idx.size(); ++k) {
            size_t v = idx[k];
            if (v >= (size_t)p.vertexCount) continue;
            float* x = &p.pos[v * 3];
            float* n = &p.nrm[v * 3];
            for (int c = 0; c < 3; ++c) { x[c] += dp[k * 3 + c]; n[c] += dn[k * 3 + c]; }
            float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (l > 1e-6f) for (int c = 0; c < 3; ++c) n[c] /= l;
            p.bmin = V3{std::min(p.bmin.x, x[0]), std::min(p.bmin.y, x[1]), std::min(p.bmin.z, x[2])};
            p.bmax = V3{std::max(p.bmax.x, x[0]), std::max(p.bmax.y, x[1]), std::max(p.bmax.z, x[2])};
        }
        // the new bind shape on the GPU (morphing re-uploads from these base arrays afterwards)
        for (int slot = 0; slot < 2; ++slot) {
            const std::vector<float>& d = slot == 0 ? p.pos : p.nrm;
            if (!p.vbo[slot]) continue;
            glBindBuffer(GL_ARRAY_BUFFER, p.vbo[slot]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(d.size() * sizeof(float)), d.data());
        }
        moved += en.n;
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    AG_INFO("AG: identity %s (%s): %d vertices", head["model"].str().c_str(), file.c_str(), moved);
    return true;
}

}  // namespace ag
