# Afterglow — native game (C++17, raylib 5.5). Builds on GitHub Actions (see .github/workflows/native.yml).
cmake_minimum_required(VERSION 3.21)
project(Afterglow VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # static CRT: no redistributable needed
endif()
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# ---- dependencies ----
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(raygui
  URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR _header_only_)          # header-only: nothing to add_subdirectory
FetchContent_MakeAvailable(raylib raygui)

# ---- shaders embedded as byte arrays (no string-literal size limits) ----
set(AG_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB AG_SHADER_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag)
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
file(WRITE ${AG_GEN_DIR}/embedded_shaders.h.in
  "// generated from native/shaders by CMake - do not edit\n#pragma once\n"
  "struct EmbeddedShader { const char* name; const char* source; };\n"
  "${AG_ARRAYS}static const EmbeddedShader kEmbeddedShaders[] = {\n${AG_TABLE}};\n"
  "static const int kEmbeddedShaderCount = ${AG_SHADER_COUNT};\n")
configure_file(${AG_GEN_DIR}/embedded_shaders.h.in ${AG_GEN_DIR}/embedded_shaders.h COPYONLY)

# ---- game executable ----
file(GLOB_RECURSE AG_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(afterglow ${AG_SOURCES})
target_include_directories(afterglow PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src ${AG_GEN_DIR}
  ${raylib_SOURCE_DIR}/src ${raylib_SOURCE_DIR}/src/external ${raygui_SOURCE_DIR}/src)
target_link_libraries(afterglow PRIVATE raylib)
set_target_properties(afterglow PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<0:>)

if(MSVC)
  target_compile_options(afterglow PRIVATE /utf-8 /W3 /permissive- /wd4244 /wd4267 /wd4305 /wd4996 /wd4018)
  target_compile_definitions(afterglow PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  set_target_properties(afterglow PROPERTIES OUTPUT_NAME Afterglow WIN32_EXECUTABLE TRUE INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  target_link_options(afterglow PRIVATE /ENTRY:mainCRTStartup)
  target_sources(afterglow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/installer/afterglow.rc)
else()
  target_compile_options(afterglow PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-enum-compare)
endif()

# ---- assets next to the executable (legacy web assets fill in until native/assets has them) ----
add_custom_command(TARGET afterglow POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DREPO=${CMAKE_CURRENT_SOURCE_DIR}/.. -DDST=$<TARGET_FILE_DIR:afterglow>
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_assets.cmake
  VERBATIM)
