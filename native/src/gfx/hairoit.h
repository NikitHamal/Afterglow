// Afterglow engine — order-independent soft hair (weighted blended OIT, McGuire & Bavoil 2013).
// Hair cards draw their dense strand cores opaque in the main pass; everything softer (the translucent sheet under
// the strands, strand tips, wisps, the cap's hairline fade) is accumulated here and composited over the frame, so
// overlapping cards average instead of fighting over draw order (no "scraps", no slab-like card silhouettes).
// GL 3.3 core: one blend function serves both targets — colour/weight add, revealage multiplies in target 0's alpha.
#pragma once
#include "gfx/shader.h"

namespace ag {

class HairOit {
public:
    bool build();                                   // the composite program
    void destroy();
    void ensure(int w, int h, GLuint depthTex);     // targets at the HDR size, sharing its depth (test only)
    void begin();                                   // bind, clear, blend state; draw soft hair with u_pass = 1
    void end(GLuint hdrFbo, int w, int h, GLuint emptyVao);   // composite over the HDR frame
    bool ready() const { return fbo_ != 0 && prog_.valid(); }

private:
    GLuint fbo_ = 0, accum_ = 0, weight_ = 0, depth_ = 0;
    int w_ = 0, h_ = 0;
    Program prog_;
};

}  // namespace ag
