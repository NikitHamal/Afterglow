// Afterglow engine — glTF 2.0 model: nodes, meshes (32-bit indices, morph targets), PBR materials,
// skins, animation clips, punctual lights and extras. Loaded with cgltf (linked from raylib).
#pragma once
#include "gfx/gl.h"
#include "core/mathx.h"
#include "core/json.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

namespace ag {

enum class AlphaMode { Opaque = 0, Mask = 1, Blend = 2 };
// Shading model chosen per material (by name heuristics, overridable).
enum class Shading { Generic = 0, Skin = 1, Hair = 2, Eye = 3, Room = 4, Unlit = 5 };

struct Material {
    std::string name;
    V4 baseColor{1, 1, 1, 1};
    float metallic = 1.f, roughness = 1.f, normalScale = 1.f, occlusion = 1.f;
    V3 emissive{0, 0, 0};
    GLuint texBase = 0, texNormal = 0, texMR = 0, texOcc = 0, texEmis = 0;
    int uvBase = 0, uvNormal = 0, uvMR = 0, uvOcc = 0, uvEmis = 0;
    AlphaMode alpha = AlphaMode::Opaque;
    float cutoff = 0.5f;
    bool doubleSided = false, unlit = false;
    Shading shading = Shading::Generic;
    bool castShadow = true;
    Json extras;
};

struct Primitive {
    GLuint vao = 0, ebo = 0;
    GLuint vbo[7] = {0, 0, 0, 0, 0, 0, 0};   // 0 pos,1 nrm,2 uv0,3 uv1,4 tan,5 joints,6 weights
    int indexCount = 0, vertexCount = 0;
    int material = -1;
    bool skinned = false, hasUv1 = false, dynamic = false;
    std::vector<float> pos, nrm, tan;                 // base data (pos/nrm kept for morphing + bounds)
    std::vector<std::vector<float>> dPos, dNrm;       // per morph target while loading (dense, then packed)
    // packed morph targets: only the vertices a target moves (face shapes touch a few hundred of ~40k)
    struct Sparse { std::vector<uint32_t> idx; std::vector<float> dp, dn; };
    std::vector<Sparse> targets;
    int targetCount = 0;
    uint32_t dirtyLo = 0, dirtyHi = 0;   // vertex range uploaded last time (reset to base this time)
    std::vector<float> scratchPos, scratchNrm;
    std::vector<uint16_t> joints;                     // 4 per vertex (CPU queries)
    std::vector<float> weights;                       // 4 per vertex
    std::vector<uint32_t> idx;                        // kept for CPU queries (e.g. collision checks)
    V3 bmin{0, 0, 0}, bmax{0, 0, 0};
};

struct Mesh {
    std::string name;
    std::vector<Primitive> prims;
    std::vector<std::string> targetNames;
    std::vector<float> defaultWeights;
};

struct Node {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    V3 t{0, 0, 0};
    Quat r;
    V3 s{1, 1, 1};
    int mesh = -1, skin = -1, light = -1;
    Json extras;
};

struct Skin {
    std::vector<int> joints;
    std::vector<M4> invBind;
};

struct Channel {
    int node = -1;
    int path = 0;          // 0 translation, 1 rotation, 2 scale, 3 weights
    int interp = 0;        // 0 linear, 1 step, 2 cubic spline
    int comps = 0;         // floats per output element (3, 4 or morph target count)
    std::vector<float> times, values;
};

struct Clip {
    std::string name;
    float duration = 0.f;
    std::vector<Channel> channels;
};

struct Light {
    std::string name;
    int type = 1;          // 0 directional, 1 point, 2 spot
    V3 color{1, 1, 1};
    float intensity = 1.f, range = 0.f, inner = 0.f, outer = 0.785f;
    int node = -1;
};

class Model {
public:
    bool load(const std::string& path);
    void destroy();
    ~Model() { destroy(); }

    // Build GPU buffers for a primitive whose CPU arrays (pos/nrm/uv/idx) were filled manually.
    static void uploadPrimitive(Primitive& p, const std::vector<float>& uv0, const std::vector<float>& uv1);
    // Re-upload morphed positions/normals (dynamic primitives only).
    static void updateMorph(Primitive& p, const float* weights, int count);
    // after load-time queries: drop CPU copies the GPU already has (keeps what morphing needs)
    void releaseCpuData();
    static long long bufferBytes();      // GPU vertex/index bytes uploaded by all models
    // called on the main thread while worker threads decode textures (keep the window alive, redraw a loader)
    static std::function<void()> loadPump;

    int findNode(const std::string& name) const;
    int findClip(const std::string& name) const;
    int findMaterial(const std::string& name) const;
    void localBounds(V3& mn, V3& mx) const;   // union of primitive bounds (raw mesh space)

    std::string path;
    std::vector<Node> nodes;
    std::vector<int> roots;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Skin> skins;
    std::vector<Clip> clips;
    std::vector<Light> lights;
    std::vector<GLuint> textures;      // owned
    Json sceneExtras;
    std::unordered_map<std::string, int> nodeIndex;
    int maxJoints = 0;
};

// Tangent generation (per-vertex, orthonormalised, w = handedness).
void computeTangents(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<float>& uv,
                     const std::vector<uint32_t>& idx, std::vector<float>& tanOut);

}  // namespace ag
