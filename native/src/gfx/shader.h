// Afterglow engine — GLSL program wrapper with cached uniform locations.
#pragma once
#include "gfx/gl.h"
#include "core/mathx.h"
#include <string>
#include <unordered_map>

namespace ag {

class Program {
public:
    bool build(const std::string& name, const std::string& vs, const std::string& fs);
    void destroy();
    void use() const;
    bool valid() const { return id_ != 0; }
    GLuint id() const { return id_; }
    GLint loc(const char* name);

    void set(const char* n, int v);
    void set(const char* n, float v);
    void set(const char* n, const V2& v);
    void set(const char* n, const V3& v);
    void set(const char* n, const V4& v);
    void set(const char* n, const M4& m);
    void setArray(const char* n, const V3* v, int count);
    void setArray(const char* n, const float* v, int count);
    void sampler(const char* n, int unit, GLuint tex, GLenum target = GL_TEXTURE_2D);

private:
    GLuint id_ = 0;
    std::string name_;
    std::unordered_map<const char*, GLint> locs_;   // keyed by literal address (no per-call allocation)
};

// Shader sources: embedded at build time, overridable at runtime from <assets>/../shaders for hot reload.
std::string shaderSource(const std::string& file);

}  // namespace ag
