cmake_minimum_required(VERSION 4.1)

project(
  HandmadeHero
  VERSION 1.0
  DESCRIPTION
  "Project following Handmade Hero Tutorials [@MollyRocket]"
  LANGUAGES
  CXX
)

option(ARCH_32BIT "Build for 32bit platform" OFF)

set(CMAKE_BUILD_TYPE RelWithDebInfo)

string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M%S")

add_library(handmade SHARED handmade.cpp)
set_target_properties(
  handmade
  PROPERTIES
  WIN32_EXECUTABLE
  TRUE
  PDB_NAME
  "handmade_${BUILD_TIMESTAMP}"
)
target_compile_definitions(
  handmade
  PUBLIC
  HANDMADE_SLOW=1
  HANDMADE_INTERNAL=1
)
target_compile_options(
  handmade
  BEFORE
  PRIVATE
  -MTd
  -nologo
  -Gm-
  -GR-
  -EHa-
  -Od
  -Oi
  -WX
  -W4
  -wd4201
  -wd4100
  -wd4189
  -wd4456
)
if(ARCH_32BIT)
  target_link_options(
    handmade
    PRIVATE
    -subsystem:windows
    -version:5.1
    "/EXPORT:GameUpdateAndRender"
    "/EXPORT:GameGetSoundSamples"
    -incremental:no
  )
else()
  target_link_options(
    handmade
    PRIVATE
    "/EXPORT:GameUpdateAndRender"
    "/EXPORT:GameGetSoundSamples"
    -incremental:no
  )
endif()
target_link_libraries(handmade)

add_executable(win32_handmade win32_handmade.cpp)
set_target_properties(
  win32_handmade
  PROPERTIES
  WIN32_EXECUTABLE
  TRUE
  PDB_NAME
  "win32_handmade_${BUILD_TIMESTAMP}"
)
target_compile_definitions(
  win32_handmade
  PUBLIC
  HANDMADE_SLOW=1
  HANDMADE_INTERNAL=1
)
target_compile_options(
  win32_handmade
  BEFORE
  PRIVATE
  -MT
  -nologo
  -Gm-
  -GR-
  -EHa-
  -Od
  -Oi
  -WX
  -W4
  -wd4201
  -wd4100
  -wd4189
  -wd4456
)
if(ARCH_32BIT)
  target_link_options(
    win32_handmade
    PRIVATE
    -subsystem:windows
    -version:5.1
    -incremental:no
  )
else()
  target_link_options(win32_handmade PRIVATE -incremental:no)
endif()
target_link_libraries(win32_handmade user32 gdi32 winmm)
