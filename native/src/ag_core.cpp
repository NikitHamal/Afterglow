// Afterglow native — ag_core.cpp (defaults + util).
#include "ag_core.h"

#include <cstdio>

namespace ag {

void Game::reset() { *this = Game(); }

std::string lerpHex(std::string_view a, std::string_view b, F64 t) {
  ColorF ca = mustColor(a), cb = mustColor(b);
  t = clamp(t, 0.0, 1.0);
  char buf[8];
  std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", toU8((float)lerp(ca.r, cb.r, t)),
                toU8((float)lerp(ca.g, cb.g, t)), toU8((float)lerp(ca.b, cb.b, t)));
  return std::string(buf);
}

} // namespace ag
