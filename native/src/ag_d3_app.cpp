// Afterglow native — ag_d3_app.cpp
// 3D app frame (1:1 with js3d/app3d.js tick3/boot3) + d3gl runtime GL loader.
//
// Loader: defines every `extern` d3gl:: entry point from ag_d3_gl.h and
// resolves them in d3gl::load() through the caller-supplied getter
// (SDL_GL_GetProcAddress on desktop Windows + Android SDL). Any missing
// symbol fails gracefully (false), except ClearDepthf which falls back to
// desktop glClearDepth(double) through a file-local adapter.
//
// App3d: init engine+cast+fx for real; frame() is the shared sim step plus
// the 3D animation/pose/draw stages:
//   app::step (js/mechanics.js, unchanged — like 3d.html reusing js/*.js)
//   + animUpdate (anim module, js3d/anim3d.js updateAnim3)
//   + cast.pose (js3d/poses3d.js) + fx.update + engine/cast/fx draws.
// The platform owns the window + GL context (ag_platform.cpp --3d branch);
// Engine only issues GL calls between beginFrame/endFrame.
#include "ag_d3_gl.h"
#include "ag_3d.h"
#include "ag_2dmods.h"

namespace ag::d3 {
// Owned + defined by the anim module (1:1 js3d/anim3d.js updateAnim3).
// Forward declaration only — do NOT define here.
void animUpdate(Game& g, double dt);
} // namespace ag::d3

namespace {
// File-local loader helpers (kept out of ag::d3gl's public surface).
template <typename T>
bool resolveGl(T& fn, ag::d3gl::GetProc get, const char* name) {
  fn = reinterpret_cast<T>(get(name));
  return fn != nullptr;
}

// Desktop-GL fallback source for ClearDepthf: glClearDepth(double).
void (*gClearDepthD)(double) = nullptr;
void clearDepthfViaD(ag::d3gl::GLfloat d) {
  if (gClearDepthD) gClearDepthD(static_cast<double>(d));
}
} // namespace

namespace ag::d3gl {

void (*Viewport)(GLint x, GLint y, GLsizei w, GLsizei h) = nullptr;
void (*Scissor)(GLint x, GLint y, GLsizei w, GLsizei h) = nullptr;
void (*Clear)(GLbitfield mask) = nullptr;
void (*ClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a) = nullptr;
void (*ClearDepthf)(GLfloat d) = nullptr;
void (*Enable)(GLenum cap) = nullptr;
void (*Disable)(GLenum cap) = nullptr;
void (*BlendFunc)(GLenum sf, GLenum df) = nullptr;
void (*DepthFunc)(GLenum f) = nullptr;
void (*DepthMask)(GLboolean flag) = nullptr;
void (*CullFace)(GLenum mode) = nullptr;
void (*PixelStorei)(GLenum pname, GLint param) = nullptr;
GLenum (*GetError)() = nullptr;
const GLubyte* (*GetString)(GLenum name) = nullptr;
GLuint (*CreateShader)(GLenum type) = nullptr;
void (*ShaderSource)(GLuint s, GLsizei n, const GLchar* const* str,
                     const GLint* len) = nullptr;
void (*CompileShader)(GLuint s) = nullptr;
void (*GetShaderiv)(GLuint s, GLenum p, GLint* v) = nullptr;
void (*GetShaderInfoLog)(GLuint s, GLsizei max, GLsizei* len,
                         GLchar* log) = nullptr;
void (*DeleteShader)(GLuint s) = nullptr;
GLuint (*CreateProgram)() = nullptr;
void (*AttachShader)(GLuint p, GLuint s) = nullptr;
void (*LinkProgram)(GLuint p) = nullptr;
void (*GetProgramiv)(GLuint p, GLenum q, GLint* v) = nullptr;
void (*GetProgramInfoLog)(GLuint p, GLsizei max, GLsizei* len,
                          GLchar* log) = nullptr;
void (*UseProgram)(GLuint p) = nullptr;
void (*DeleteProgram)(GLuint p) = nullptr;
GLint (*GetAttribLocation)(GLuint p, const GLchar* n) = nullptr;
GLint (*GetUniformLocation)(GLuint p, const GLchar* n) = nullptr;
void (*EnableVertexAttribArray)(GLuint i) = nullptr;
void (*VertexAttribPointer)(GLuint i, GLint size, GLenum type, GLboolean norm,
                           GLsizei stride, const void* ptr) = nullptr;
void (*DisableVertexAttribArray)(GLuint i) = nullptr;
void (*UniformMatrix4fv)(GLint l, GLsizei n, GLboolean t,
                         const GLfloat* v) = nullptr;
void (*Uniform4fv)(GLint l, GLsizei n, const GLfloat* v) = nullptr;
void (*Uniform3fv)(GLint l, GLsizei n, const GLfloat* v) = nullptr;
void (*Uniform1f)(GLint l, GLfloat v) = nullptr;
void (*Uniform1i)(GLint l, GLint v) = nullptr;
void (*GenBuffers)(GLsizei n, GLuint* b) = nullptr;
void (*BindBuffer)(GLenum t, GLuint b) = nullptr;
void (*BufferData)(GLenum t, GLsizeiptr s, const void* d, GLenum u) = nullptr;
void (*BufferSubData)(GLenum t, GLintptr o, GLsizeiptr s,
                      const void* d) = nullptr;
void (*DeleteBuffers)(GLsizei n, const GLuint* b) = nullptr;
void (*GenTextures)(GLsizei n, GLuint* t) = nullptr;
void (*BindTexture)(GLenum t, GLuint x) = nullptr;
void (*TexImage2D)(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLint b,
                   GLenum f2, GLenum ty, const void* d) = nullptr;
void (*TexParameteri)(GLenum t, GLenum p, GLint v) = nullptr;
void (*DeleteTextures)(GLsizei n, const GLuint* t) = nullptr;
void (*ActiveTexture)(GLenum t) = nullptr;
void (*DrawArrays)(GLenum m, GLint f, GLsizei c) = nullptr;
void (*DrawElements)(GLenum m, GLsizei c, GLenum t, const void* i) = nullptr;

bool load(GetProc get) {
  if (!get) return false;
  gClearDepthD = nullptr;
  // Every resolve runs (no short-circuit): a later load() must never see
  // stale pointers from an earlier one. Each assignment overwrites, even
  // with nullptr on missing symbols.
  resolveGl(Viewport, get, "glViewport");
  resolveGl(Scissor, get, "glScissor");
  resolveGl(Clear, get, "glClear");
  resolveGl(ClearColor, get, "glClearColor");
  resolveGl(Enable, get, "glEnable");
  resolveGl(Disable, get, "glDisable");
  resolveGl(BlendFunc, get, "glBlendFunc");
  resolveGl(DepthFunc, get, "glDepthFunc");
  resolveGl(DepthMask, get, "glDepthMask");
  resolveGl(CullFace, get, "glCullFace");
  resolveGl(PixelStorei, get, "glPixelStorei");
  resolveGl(GetError, get, "glGetError");
  resolveGl(GetString, get, "glGetString");
  resolveGl(CreateShader, get, "glCreateShader");
  resolveGl(ShaderSource, get, "glShaderSource");
  resolveGl(CompileShader, get, "glCompileShader");
  resolveGl(GetShaderiv, get, "glGetShaderiv");
  resolveGl(GetShaderInfoLog, get, "glGetShaderInfoLog");
  resolveGl(DeleteShader, get, "glDeleteShader");
  resolveGl(CreateProgram, get, "glCreateProgram");
  resolveGl(AttachShader, get, "glAttachShader");
  resolveGl(LinkProgram, get, "glLinkProgram");
  resolveGl(GetProgramiv, get, "glGetProgramiv");
  resolveGl(GetProgramInfoLog, get, "glGetProgramInfoLog");
  resolveGl(UseProgram, get, "glUseProgram");
  resolveGl(DeleteProgram, get, "glDeleteProgram");
  resolveGl(GetAttribLocation, get, "glGetAttribLocation");
  resolveGl(GetUniformLocation, get, "glGetUniformLocation");
  resolveGl(EnableVertexAttribArray, get, "glEnableVertexAttribArray");
  resolveGl(VertexAttribPointer, get, "glVertexAttribPointer");
  resolveGl(DisableVertexAttribArray, get, "glDisableVertexAttribArray");
  resolveGl(UniformMatrix4fv, get, "glUniformMatrix4fv");
  resolveGl(Uniform4fv, get, "glUniform4fv");
  resolveGl(Uniform3fv, get, "glUniform3fv");
  resolveGl(Uniform1f, get, "glUniform1f");
  resolveGl(Uniform1i, get, "glUniform1i");
  resolveGl(GenBuffers, get, "glGenBuffers");
  resolveGl(BindBuffer, get, "glBindBuffer");
  resolveGl(BufferData, get, "glBufferData");
  resolveGl(BufferSubData, get, "glBufferSubData");
  resolveGl(DeleteBuffers, get, "glDeleteBuffers");
  resolveGl(GenTextures, get, "glGenTextures");
  resolveGl(BindTexture, get, "glBindTexture");
  resolveGl(TexImage2D, get, "glTexImage2D");
  resolveGl(TexParameteri, get, "glTexParameteri");
  resolveGl(DeleteTextures, get, "glDeleteTextures");
  resolveGl(ActiveTexture, get, "glActiveTexture");
  resolveGl(DrawArrays, get, "glDrawArrays");
  resolveGl(DrawElements, get, "glDrawElements");
  // ClearDepthf: GLES2 entry first; desktop GL only has glClearDepth(double),
  // wrapped by the file-local adapter above.
  if (!resolveGl(ClearDepthf, get, "glClearDepthf")) {
    if (resolveGl(gClearDepthD, get, "glClearDepth"))
      ClearDepthf = &clearDepthfViaD;
    else
      ClearDepthf = nullptr;
  }
  // Re-check: graceful false when anything is still missing.
  return Viewport && Scissor && Clear && ClearColor && ClearDepthf && Enable &&
         Disable && BlendFunc && DepthFunc && DepthMask && CullFace &&
         PixelStorei && GetError && GetString && CreateShader &&
         ShaderSource && CompileShader && GetShaderiv && GetShaderInfoLog &&
         DeleteShader && CreateProgram && AttachShader && LinkProgram &&
         GetProgramiv && GetProgramInfoLog && UseProgram && DeleteProgram &&
         GetAttribLocation && GetUniformLocation && EnableVertexAttribArray &&
         VertexAttribPointer && DisableVertexAttribArray &&
         UniformMatrix4fv && Uniform4fv && Uniform3fv && Uniform1f &&
         Uniform1i && GenBuffers && BindBuffer && BufferData &&
         BufferSubData && DeleteBuffers && GenTextures && BindTexture &&
         TexImage2D && TexParameteri && DeleteTextures && ActiveTexture &&
         DrawArrays && DrawElements;
}

} // namespace ag::d3gl

namespace ag::d3 {

bool App3d::init(const char* assetDir, int w, int h) {
  lookMode = false;
  if (!eng.init(w, h)) return false; // GL context owned by the platform
  // Model load is best-effort: the procedural rig (js3d/chars3d.js) draws
  // even when no GLB asset is present, so a missing file never kills --3d.
  cast.loadAll(assetDir && *assetDir ? assetDir : "assets");
  fx = Fx{};
  return true;
}

void App3d::frame(Game& g, Rng& rng, double dt) {
  // Mirrors js3d/app3d.js tick3 order: shared sim, animation, pose, fx,
  // camera+clear, draws. HUD + buffer swap stay in the platform layer.
  app::step(g, rng, dt); // js/mechanics.js — unchanged shared sim
  animUpdate(g, dt);     // js3d/anim3d.js updateAnim3 (anim module)
  cast.pose(g, dt);      // js3d/poses3d.js pose solve
  fx.update(g, dt);      // js3d/fx3d.js particles
  eng.beginFrame();      // js3d/engine3d.js camera + clear
  cast.draw(eng, g);     // js3d/chars3d.js (+glbModel when active)
  fx.draw(eng, g);
  eng.endFrame();
}

void App3d::shutdown() {
  cast.her.unload();
  cast.him.unload();
  eng.shutdown();
}

} // namespace ag::d3
