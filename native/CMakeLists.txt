cmake_minimum_required(VERSION 3.28)

# Afterglow native port (C++20) — Windows PC + Android from one tree.
# Pixel-parity strategy: fixed 1280x720 framebuffer, software Canvas2D
# rasterizer with HTML5 semantics (ag_canvas), 1:1 ports of js/*.js.
project(Afterglow VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AG_WITH_SDL "SDL3 platform layer (window/input/audio). OFF = headless only" ON)
option(AG_BUILD_PARITY "Build ag_parity headless pixel harness" ON)
option(AG_LTO "Link-time optimization in Release" ON)

if(AG_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
  if(_ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  endif()
endif()

include(FetchContent)

# Header-only third party without CMakeLists (stb, cgltf): download the
# single headers once into the build tree. Pinned to master blobs; hashes
# are verified by CI (see .github/workflows) — deterministic enough for art.
set(AG_VENDOR_DIR "${CMAKE_BINARY_DIR}/vendor")
file(MAKE_DIRECTORY "${AG_VENDOR_DIR}")
function(ag_fetch name url)
  if(NOT EXISTS "${AG_VENDOR_DIR}/${name}")
    file(DOWNLOAD "${url}" "${AG_VENDOR_DIR}/${name}" TLS_VERIFY ON)
  endif()
endfunction()
ag_fetch(stb_vorbis.c
  https://raw.githubusercontent.com/nothings/stb/master/stb_vorbis.c)
ag_fetch(stb_truetype.h
  https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h)
ag_fetch(cgltf.h
  https://raw.githubusercontent.com/jkuhlmann/cgltf/master/cgltf.h)

if(AG_WITH_SDL AND NOT ANDROID)
  # Pinned SDL3 release; Android flow vendors SDL via android/ overlay instead.
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.2.10.zip)
  FetchContent_MakeAvailable(SDL3)
endif()

# Google Fonts (OFL) fetched once at configure for HUD parity with css/app.css.
set(AG_FONT_DIR "${CMAKE_BINARY_DIR}/fonts")
file(MAKE_DIRECTORY "${AG_FONT_DIR}")
if(NOT EXISTS "${AG_FONT_DIR}/Sora-Regular.ttf")
  file(DOWNLOAD
    https://github.com/google/fonts/raw/main/ofl/sora/Sora%5Bwght%5D.ttf
    "${AG_FONT_DIR}/Sora-Regular.ttf" TLS_VERIFY ON)
endif()
if(NOT EXISTS "${AG_FONT_DIR}/CormorantGaramond-Medium.ttf")
  file(DOWNLOAD
    https://github.com/google/fonts/raw/main/ofl/cormorantgaramond/CormorantGaramond%5Bwght%5D.ttf
    "${AG_FONT_DIR}/CormorantGaramond-Medium.ttf" TLS_VERIFY ON)
endif()

add_library(afterglow_core STATIC
  src/ag_canvas.cpp
  src/ag_core.cpp
  src/ag_gfx.cpp
  src/ag_skin.cpp
  src/ag_mech.cpp
  src/ag_input.cpp
  src/ag_poses.cpp
  src/ag_side.cpp
  src/ag_fpv.cpp
  src/ag_oral.cpp
  src/ag_chars.cpp
  src/ag_audio.cpp
  src/ag_hud.cpp
  src/ag_d3_engine.cpp
  src/ag_d3_glb.cpp
  src/ag_d3_cast.cpp
  src/ag_d3_animfx.cpp
  src/ag_d3_app.cpp
  src/ag_port_todo.cpp
  src/ag_3d_todo.cpp
)
target_include_directories(afterglow_core PUBLIC
  src
  "${AG_VENDOR_DIR}"
)
target_compile_definitions(afterglow_core PUBLIC AG_VW=1280 AG_VH=720)

if(MSVC)
  target_compile_options(afterglow_core PRIVATE /W4 /permissive- /EHsc /O2 /Ot)
else()
  target_compile_options(afterglow_core PRIVATE -O3 -Wall -Wextra -ffast-math
    -fno-math-errno)
endif()

if(AG_WITH_SDL AND NOT ANDROID)
  add_executable(afterglow WIN32 src/main.cpp src/ag_platform.cpp)
  target_link_libraries(afterglow PRIVATE afterglow_core SDL3::SDL3)
  if(TARGET SDL3::SDL3main)
    target_link_libraries(afterglow PRIVATE SDL3::SDL3main)
  endif()
  target_compile_definitions(afterglow PRIVATE AG_WITH_SDL=1)
else()
  # Headless binary (CI parity, no window): same main, null platform.
  add_executable(afterglow src/main.cpp src/ag_platform.cpp)
endif()

if(AG_BUILD_PARITY)
  # headlessRun lives in ag_platform.cpp (headless branch, no SDL), which is
  # otherwise only compiled into the `afterglow` exe — so the parity harness
  # must compile it too, otherwise ag_parity fails to link (LNK2001). This
  # keeps one definition of headlessRun shared by both binaries.
  add_executable(ag_parity tests/ag_parity.cpp src/ag_platform.cpp)
  target_link_libraries(ag_parity PRIVATE afterglow_core)
  if(AG_WITH_SDL AND NOT ANDROID)
    target_link_libraries(ag_parity PRIVATE SDL3::SDL3)
    target_include_directories(ag_parity PRIVATE
      "${CMAKE_BINARY_DIR}/_deps/sdl3-src/include")
    target_compile_definitions(ag_parity PRIVATE AG_WITH_SDL=1)
  endif()
endif()

# ---- install / packaging (Windows) ----
install(TARGETS afterglow RUNTIME DESTINATION .)
install(DIRECTORY "${AG_FONT_DIR}/" DESTINATION fonts)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/../assets" DESTINATION assets
  PATTERN "*.blend" EXCLUDE)
set(CPACK_GENERATOR "ZIP")
if(WIN32)
  list(APPEND CPACK_GENERATOR "NSIS")
  set(CPACK_NSIS_MODIFY_PATH OFF)
endif()
set(CPACK_PACKAGE_NAME "Afterglow")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
include(CPack)
