# Afterglow — Android (arm64 + x86_64) native build.
# Compiles the engine sources and raylib (GLES 3 backend) into the NativeActivity library "afterglow".
cmake_minimum_required(VERSION 3.22)
project(afterglow C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(AG_NATIVE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)   # native/
set(AG_ROOT ${AG_NATIVE}/..)
set(AG_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${AG_GEN})

# ---- raylib 5.5 (fetched once; GLES3 backend, Android platform) ----
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
FetchContent_GetProperties(raylib)
if(NOT raylib_POPULATED)
  FetchContent_Populate(raylib)
endif()
set(RL_SRC ${raylib_SOURCE_DIR}/src)

# Two edits raylib needs on Android: a real ES3 EGL context (its Android backend always asks for ES2), and a left
# mouse press from touch so the game's mouse-driven UI and camera drag work. Text replacement keeps the fetch free
# of patch tools.
set(RL_ANDROID ${RL_SRC}/platforms/rcore_android.c)
file(READ ${RL_ANDROID} RL_TEXT)
string(REPLACE
  "    if (CORE.Input.Touch.pointCount > 0) CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 1;
    else CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 0;"
  "    if (CORE.Input.Touch.pointCount > 0) CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 1;
    else CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 0;
    // Afterglow: a touch is also a left mouse press, so the mouse-driven UI works on a phone
    CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = (CORE.Input.Touch.pointCount > 0);"
  RL_TEXT "${RL_TEXT}")
string(REPLACE
  "        EGL_CONTEXT_CLIENT_VERSION, 2,"
  "#if defined(GRAPHICS_API_OPENGL_ES3)
        EGL_CONTEXT_CLIENT_VERSION, 3,   // Afterglow: ES3 context for GLSL ES 300
#else
        EGL_CONTEXT_CLIENT_VERSION, 2,
#endif"
  RL_TEXT "${RL_TEXT}")
if(NOT RL_TEXT MATCHES "Afterglow: ES3 context" OR NOT RL_TEXT MATCHES "Afterglow: a touch is also a left mouse press")
  message(FATAL_ERROR "raylib Android edits did not apply (its source changed)")
endif()
file(WRITE ${RL_ANDROID} "${RL_TEXT}")

# raylib's rlgl calls the extension entry point for MRT even on ES3, where the core symbol is the bindable one
set(RL_RLGL ${RL_SRC}/rlgl.h)
file(READ ${RL_RLGL} RL_RLGL_TEXT)
string(REPLACE
  "#if defined(GRAPHICS_API_OPENGL_ES3)
            glDrawBuffersEXT(count, buffers);"
  "#if defined(GRAPHICS_API_OPENGL_ES3)
            glDrawBuffers(count, buffers);"
  RL_RLGL_TEXT "${RL_RLGL_TEXT}")
if(NOT RL_RLGL_TEXT MATCHES "glDrawBuffers\(count, buffers\)")
  message(FATAL_ERROR "rlgl ES3 draw-buffers edit did not apply (raylib source changed)")
endif()
file(WRITE ${RL_RLGL} "${RL_RLGL_TEXT}")

add_library(raylib STATIC
  ${RL_SRC}/raudio.c ${RL_SRC}/rcore.c ${RL_SRC}/rmodels.c ${RL_SRC}/rshapes.c
  ${RL_SRC}/rtext.c ${RL_SRC}/rtextures.c ${RL_SRC}/utils.c)
target_include_directories(raylib PUBLIC ${RL_SRC} ${ANDROID_NDK}/sources/android/native_app_glue)   # no src/external: its Windows dirent.h would shadow the NDK one
target_compile_definitions(raylib PUBLIC PLATFORM_ANDROID GRAPHICS_API_OPENGL_ES3)
target_compile_options(raylib PRIVATE -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)

# ---- raygui 4.0 (header only) ----
FetchContent_Declare(raygui
  URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz)
FetchContent_GetProperties(raygui)
if(NOT raygui_POPULATED)
  FetchContent_Populate(raygui)
endif()

# ---- native_app_glue (NDK) ----
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

# ---- shaders: GLES 300 es variants ----
file(GLOB AG_SHADER_FILES CONFIGURE_DEPENDS ${AG_NATIVE}/shaders_es/*.vert ${AG_NATIVE}/shaders_es/*.frag)
set(AG_ARRAYS "")
set(AG_TABLE "")
foreach(f ${AG_SHADER_FILES})
  get_filename_component(name ${f} NAME)
  string(MAKE_C_IDENTIFIER ${name} id)
  file(READ ${f} hex HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
  string(APPEND AG_ARRAYS "static const char ag_shader_${id}[] = {${hex}0x00};\n")
  string(APPEND AG_TABLE "  {\"${name}\", ag_shader_${id}},\n")
endforeach()
list(LENGTH AG_SHADER_FILES AG_SHADER_COUNT)
file(WRITE ${AG_GEN}/embedded_shaders.h
  "// generated from native/shaders_es by CMake - do not edit\n#pragma once\n"
  "struct EmbeddedShader { const char* name; const char* source; };\n"
  "${AG_ARRAYS}static const EmbeddedShader kEmbeddedShaders[] = {\n${AG_TABLE}};\n"
  "static const int kEmbeddedShaderCount = ${AG_SHADER_COUNT};\n")

# ---- UI fonts (OFL, native/res/fonts) ----
file(GLOB AG_FONT_FILES CONFIGURE_DEPENDS ${AG_NATIVE}/res/fonts/*.ttf)
set(AG_FONT_ARRAYS "")
set(AG_FONT_TABLE "")
foreach(f ${AG_FONT_FILES})
  get_filename_component(name ${f} NAME_WE)
  string(MAKE_C_IDENTIFIER ${name} id)
  file(READ ${f} hex HEX)
  file(SIZE ${f} fsize)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
  string(APPEND AG_FONT_ARRAYS "static const unsigned char ag_font_${id}[] = {${hex}};\n")
  string(APPEND AG_FONT_TABLE "  {\"${name}\", ag_font_${id}, ${fsize}},\n")
endforeach()
list(LENGTH AG_FONT_FILES AG_FONT_COUNT)
file(WRITE ${AG_GEN}/embedded_fonts.h
  "// generated from native/res/fonts by CMake - do not edit\n#pragma once\n"
  "struct EmbeddedFont { const char* name; const unsigned char* data; int size; };\n"
  "${AG_FONT_ARRAYS}static const EmbeddedFont kEmbeddedFonts[] = {\n${AG_FONT_TABLE}};\n"
  "static const int kEmbeddedFontCount = ${AG_FONT_COUNT};\n")

# ---- window icon (embedded, kept small) ----
file(READ ${AG_NATIVE}/installer/afterglow.png icon_hex HEX)
file(SIZE ${AG_NATIVE}/installer/afterglow.png icon_size)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," icon_hex "${icon_hex}")
file(WRITE ${AG_GEN}/embedded_icon.h
  "// generated from native/installer/afterglow.png by CMake - do not edit\n#pragma once\n"
  "static const unsigned char kAppIconPng[] = {${icon_hex}};\nstatic const int kAppIconPngSize = ${icon_size};\n")

# ---- engine ----
file(GLOB_RECURSE AG_SOURCES CONFIGURE_DEPENDS ${AG_NATIVE}/src/*.cpp)
add_library(afterglow SHARED ${AG_SOURCES})
target_include_directories(afterglow PRIVATE
  ${AG_NATIVE}/src ${AG_GEN} ${RL_SRC} ${raygui_SOURCE_DIR}/src)
target_compile_definitions(afterglow PRIVATE GRAPHICS_API_OPENGL_ES3)
target_compile_options(afterglow PRIVATE -Wno-unused-parameter -Wno-unused-variable)
find_package(Threads REQUIRED)
target_link_libraries(afterglow PRIVATE raylib native_app_glue Threads::Threads
  android log EGL GLESv3 OpenSLES dl m)
# keep the NativeActivity entry the glue defines
target_link_options(afterglow PRIVATE -u ANativeActivity_onCreate)
