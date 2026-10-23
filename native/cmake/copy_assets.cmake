# Copies native/assets next to the executable. While the content pipeline has not delivered a piece yet,
# the legacy web assets fill in (characters scaled to real height at runtime, the sound pack for audio).
#   cmake -DSRC=<native> -DREPO=<repo root> -DDST=<bin dir> -P copy_assets.cmake
file(MAKE_DIRECTORY "${DST}/assets")
if(EXISTS "${SRC}/assets")
  file(COPY "${SRC}/assets/" DESTINATION "${DST}/assets")
endif()

foreach(who vivian marcus)
  if(NOT EXISTS "${SRC}/assets/characters/${who}.glb" AND EXISTS "${REPO}/assets/${who}/${who}.glb")
    file(MAKE_DIRECTORY "${DST}/assets/legacy/${who}")
    file(GLOB files "${REPO}/assets/${who}/*.glb" "${REPO}/assets/${who}/*.jpg")
    file(COPY ${files} DESTINATION "${DST}/assets/legacy/${who}")
  endif()
endforeach()

if(NOT EXISTS "${SRC}/assets/audio")
  set(pack "${REPO}/assets/Shinlalala's Lewd Sound Pack")
  if(EXISTS "${pack}")
    file(MAKE_DIRECTORY "${DST}/assets/audio")
    file(COPY "${pack}/" DESTINATION "${DST}/assets/audio" PATTERN "coverart.png" EXCLUDE)
  endif()
  if(EXISTS "${REPO}/assets/vox")
    file(COPY "${REPO}/assets/vox" DESTINATION "${DST}/assets/audio")
  endif()
endif()
