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
add_executable(HandmadeHero win32_handmade.cpp)
set_target_properties(HandmadeHero PROPERTIES WIN32_EXECUTABLE TRUE)
target_compile_definitions(
  HandmadeHero
  PUBLIC
  HANDMADE_SLOW=1
  HANDMADE_INTERNAL=1
)
target_compile_options(
  HandmadeHero
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
  target_link_options(HandmadeHero PRIVATE -subsystem:windows -version:5.1)
endif()
target_link_libraries(HandmadeHero user32 gdi32 winmm)
