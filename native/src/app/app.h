// Afterglow engine — application interface shared by the interactive game, the sandbox and the
// screenshot harness. The gameplay track implements its own App (title, customisation, play...).
#pragma once
#include "core/spec.h"

namespace ag {

class App {
public:
    virtual ~App() = default;
    virtual bool init() = 0;                         // after the window + GL context exist
    virtual void shutdown() {}
    virtual void reset() {}                          // harness: back to a neutral state between shots
    virtual void applySpec(const Spec& spec) { (void)spec; }
    virtual void update(float dt) = 0;               // simulation + input
    virtual void render(int fbWidth, int fbHeight) = 0;   // 3D + UI into the backbuffer
    virtual bool wantsQuit() const { return false; }
};

}  // namespace ag
