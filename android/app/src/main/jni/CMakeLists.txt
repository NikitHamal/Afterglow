# Afterglow Android JNI build.
# SDL3 comes from the workflow clone (SDL_SRC) — same tree as the Java
# activity (see app/build.gradle). Single headers (stb/cgltf) are downloaded
# like in native/CMakeLists.txt so there is exactly one vendoring pattern.
cmake_minimum_required(VERSION 3.22)
project(AfterglowAndroid LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SDL_SRC "" CACHE PATH "SDL3 source dir (CI clone)")
if(NOT SDL_SRC)
  message(FATAL_ERROR "SDL_SRC not set (workflow exports it)")
endif()
add_subdirectory(${SDL_SRC} SDL3bin)

set(AG_VENDOR_DIR "${CMAKE_BINARY_DIR}/ag-vendor")
file(MAKE_DIRECTORY "${AG_VENDOR_DIR}")
# Single headers (same set as native/CMakeLists.txt ag_fetch).
set(AG_DEPS
  "stb_vorbis.c|https://raw.githubusercontent.com/nothings/stb/master/stb_vorbis.c"
  "stb_truetype.h|https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h"
  "cgltf.h|https://raw.githubusercontent.com/jkuhlmann/cgltf/master/cgltf.h")
foreach(pair IN LISTS AG_DEPS)
  string(REPLACE "|" ";" parts "${pair}")
  list(GET parts 0 nm)
  list(GET parts 1 url)
  if(NOT EXISTS "${AG_VENDOR_DIR}/${nm}")
    file(DOWNLOAD "${url}" "${AG_VENDOR_DIR}/${nm}" TLS_VERIFY ON)
  endif()
endforeach()

file(GLOB AG_SRC
  "${CMAKE_CURRENT_LIST_DIR}/../../../../../native/src/*.cpp")

add_library(main SHARED ${AG_SRC})
target_include_directories(main PRIVATE "${AG_VENDOR_DIR}")
target_compile_definitions(main PRIVATE AG_WITH_SDL=1 AG_VW=1280 AG_VH=720)
target_compile_options(main PRIVATE -O3 -ffast-math -fno-math-errno)
target_link_libraries(main SDL3::SDL3 android log)
