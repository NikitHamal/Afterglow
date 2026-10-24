// Afterglow engine — per-instance pose evaluation: clip sampling, masked layer blending, node globals,
// skin matrices (uploaded to a float texture for GPU skinning) and morph weights.
#pragma once
#include "gfx/model.h"
#include <string>
#include <vector>

namespace ag {

using BoneMask = std::vector<float>;   // per node weight 0..1 (empty = all nodes)

class Animator {
public:
    void init(Model* model);
    void destroy();

    // Frame flow: begin() resets to rest; apply clips; computeGlobals(); procedural edits + computeGlobals();
    // finish() builds joint matrices, uploads the bone texture and re-morphs dirty meshes.
    void begin();
    void applyClip(int clipIndex, float timeSeconds, float weight, const BoneMask* mask = nullptr);
    void computeGlobals(const M4& root);
    // Final pose gate: normalises malformed channels and keeps authored joints
    // inside a generous anatomical envelope before skin matrices are uploaded.
    void guardPose() { computeGlobals(root_); }
    void finish();

    // morph weights: animated value unless a user override (>=0) exists
    void setMorphOverride(int mesh, int target, float w);   // w < 0 releases the override
    float morphWeight(int mesh, int target) const;

    // local pose access (procedural layers)
    V3& t(int n) { return pose_.t[(size_t)n]; }
    Quat& r(int n) { return pose_.r[(size_t)n]; }
    V3& s(int n) { return pose_.s[(size_t)n]; }
    const Quat& restR(int n) const { return rest_.r[(size_t)n]; }
    const V3& restT(int n) const { return rest_.t[(size_t)n]; }
    const M4& global(int n) const { return global_[(size_t)n]; }
    const M4& rootMatrix() const { return root_; }
    // world rotation of a node (unit quaternion, scale removed)
    Quat worldRot(int n) const;
    // set node n's local rotation so its world rotation becomes `w` (parent world taken from globals)
    void setWorldRot(int n, const Quat& w);
    void recomputeSubtree(int n);

    GLuint boneTexture() const { return boneTex_; }
    int boneTexWidth() const { return texW_; }
    const Model* model() const { return model_; }
    BoneMask makeMask(const std::vector<std::string>& subtreeRoots) const;

private:
    void sampleChannel(const Channel& ch, float time, float* out) const;
    void globalOf(int n);
    void sanitizePose();
    static float jointLimit(const std::string& name);

    Model* model_ = nullptr;
    struct Pose { std::vector<V3> t; std::vector<Quat> r; std::vector<V3> s; } rest_, pose_;
    std::vector<M4> global_;
    std::vector<int> order_;                     // parents before children
    M4 root_ = M4::identity();
    std::vector<std::vector<float>> morphAnim_, morphUser_, morphLast_;
    std::vector<float> boneData_;
    GLuint boneTex_ = 0;
    int texW_ = 0, texH_ = 0;
    std::vector<float> scratch_;
    bool guardWarned_ = false;
};

}  // namespace ag
