// Afterglow native — ag_d3_gl.h
// Minimal GLES2/Desktop-GL ABI for the 3D port. No system GL headers are
// included anywhere (Windows MSVC only ships GL 1.1), so this header declares
// the exact entry points + enums the engine uses. The platform resolves them
// at runtime (SDL_GL_GetProcAddress) via d3gl::load(). Shaders must be
// GLSL ES 1.00 with `#ifdef GL_ES` precision guards so they compile on both
// real ES (Android) and desktop GL (Windows SDL_GL context).
#pragma once
#include <cstddef>
#include <cstdint>

namespace ag::d3gl {

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLbyte = signed char;
using GLshort = short;
using GLint = int;
using GLubyte = unsigned char;
using GLushort = unsigned short;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLclampf = float;
using GLchar = char;
using GLsizeiptr = long long;
using GLintptr = long long;

// ---- enums used by the engine ----
inline constexpr GLenum FALSE_ = 0, TRUE_ = 1;
inline constexpr GLenum ZERO = 0, ONE = 1;
inline constexpr GLenum SRC_ALPHA = 0x0302, ONE_MINUS_SRC_ALPHA = 0x0303,
                         ONE_ = 0x1, ZERO_ = 0;
inline constexpr GLenum BLEND = 0x0BE2, DEPTH_TEST = 0x0B71,
                         CULL_FACE = 0x0B44, SCISSOR_TEST = 0x0C11;
inline constexpr GLenum COLOR_BUFFER_BIT = 0x4000, DEPTH_BUFFER_BIT = 0x0100;
inline constexpr GLenum TRIANGLES = 0x0004, TRIANGLE_STRIP = 0x0005,
                         TRIANGLE_FAN = 0x0006, LINES = 0x0001,
                         POINTS = 0x0000;
inline constexpr GLenum ARRAY_BUFFER = 0x8892, ELEMENT_ARRAY_BUFFER = 0x8893;
inline constexpr GLenum STATIC_DRAW = 0x88E4, DYNAMIC_DRAW = 0x88E8;
inline constexpr GLenum FLOAT_ = 0x1406, UNSIGNED_SHORT = 0x1403,
                         UNSIGNED_BYTE = 0x1401;
inline constexpr GLenum FRAGMENT_SHADER = 0x8B30, VERTEX_SHADER = 0x8B31;
inline constexpr GLenum COMPILE_STATUS = 0x8B81, LINK_STATUS = 0x8B82,
                         INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum TEXTURE_2D = 0x0DE1, TEXTURE0 = 0x84C0;
inline constexpr GLenum RGBA = 0x1908, RGB = 0x1907, LUMINANCE = 0x1909;
inline constexpr GLenum LINEAR = 0x2601, NEAREST = 0x2600,
                         CLAMP_TO_EDGE = 0x812F;
inline constexpr GLenum TEXTURE_MIN_FILTER = 0x2801,
                         TEXTURE_MAG_FILTER = 0x2800, TEXTURE_WRAP_S = 0x2802,
                         TEXTURE_WRAP_T = 0x2803;
inline constexpr GLenum LESS = 0x0201, LEQUAL = 0x0203;
inline constexpr GLenum BACK = 0x0405, FRONT = 0x0404, FRONT_AND_BACK = 0x0408;
inline constexpr GLenum UNPACK_ALIGNMENT = 0x0CF5;
inline constexpr GLenum NO_ERROR = 0;
inline constexpr GLenum VERSION = 0x1F02, VENDOR = 0x1F00, RENDERER = 0x1F01,
                         EXTENSIONS = 0x1F03;

// ---- entry points (resolved by d3gl::load) ----
extern void (*Viewport)(GLint x, GLint y, GLsizei w, GLsizei h);
extern void (*Scissor)(GLint x, GLint y, GLsizei w, GLsizei h);
extern void (*Clear)(GLbitfield mask);
extern void (*ClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
extern void (*ClearDepthf)(GLfloat d);
extern void (*Enable)(GLenum cap);
extern void (*Disable)(GLenum cap);
extern void (*BlendFunc)(GLenum sf, GLenum df);
extern void (*DepthFunc)(GLenum f);
extern void (*DepthMask)(GLboolean flag);
extern void (*CullFace)(GLenum mode);
extern void (*PixelStorei)(GLenum pname, GLint param);
extern GLenum (*GetError)();
extern const GLubyte* (*GetString)(GLenum name);
extern GLuint (*CreateShader)(GLenum type);
extern void (*ShaderSource)(GLuint s, GLsizei n, const GLchar* const* str,
                            const GLint* len);
extern void (*CompileShader)(GLuint s);
extern void (*GetShaderiv)(GLuint s, GLenum p, GLint* v);
extern void (*GetShaderInfoLog)(GLuint s, GLsizei max, GLsizei* len,
                                GLchar* log);
extern void (*DeleteShader)(GLuint s);
extern GLuint (*CreateProgram)();
extern void (*AttachShader)(GLuint p, GLuint s);
extern void (*LinkProgram)(GLuint p);
extern void (*GetProgramiv)(GLuint p, GLenum q, GLint* v);
extern void (*GetProgramInfoLog)(GLuint p, GLsizei max, GLsizei* len,
                                 GLchar* log);
extern void (*UseProgram)(GLuint p);
extern void (*DeleteProgram)(GLuint p);
extern GLint (*GetAttribLocation)(GLuint p, const GLchar* n);
extern GLint (*GetUniformLocation)(GLuint p, const GLchar* n);
extern void (*EnableVertexAttribArray)(GLuint i);
extern void (*VertexAttribPointer)(GLuint i, GLint size, GLenum type,
                                  GLboolean norm, GLsizei stride,
                                  const void* ptr);
extern void (*DisableVertexAttribArray)(GLuint i);
extern void (*UniformMatrix4fv)(GLint l, GLsizei n, GLboolean t,
                                const GLfloat* v);
extern void (*Uniform4fv)(GLint l, GLsizei n, const GLfloat* v);
extern void (*Uniform3fv)(GLint l, GLsizei n, const GLfloat* v);
extern void (*Uniform1f)(GLint l, GLfloat v);
extern void (*Uniform1i)(GLint l, GLint v);
extern void (*GenBuffers)(GLsizei n, GLuint* b);
extern void (*BindBuffer)(GLenum t, GLuint b);
extern void (*BufferData)(GLenum t, GLsizeiptr s, const void* d, GLenum u);
extern void (*BufferSubData)(GLenum t, GLintptr o, GLsizeiptr s,
                             const void* d);
extern void (*DeleteBuffers)(GLsizei n, const GLuint* b);
extern void (*GenTextures)(GLsizei n, GLuint* t);
extern void (*BindTexture)(GLenum t, GLuint x);
extern void (*TexImage2D)(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h,
                          GLint b, GLenum f2, GLenum ty, const void* d);
extern void (*TexParameteri)(GLenum t, GLenum p, GLint v);
extern void (*DeleteTextures)(GLsizei n, const GLuint* t);
extern void (*ActiveTexture)(GLenum t);
extern void (*DrawArrays)(GLenum m, GLint f, GLsizei c);
extern void (*DrawElements)(GLenum m, GLsizei c, GLenum t, const void* i);

using GetProc = void* (*)(const char*);
// Resolves every pointer above; false if any is missing (B5 loader may
// install small fallbacks, e.g. ClearDepthf via glClearDepth, then re-check).
bool load(GetProc get);

} // namespace ag::d3gl
