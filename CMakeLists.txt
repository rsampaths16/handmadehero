cmake_minimum_required(VERSION 4.1)

project(
  HandmadeHero
  VERSION 1.0
  DESCRIPTION
  "Project following Handmade Hero Tutorials [@MollyRocket]"
  LANGUAGES
  CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_executable(HandmadeHero win32_handmade.cpp)
set_target_properties(HandmadeHero PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(HandmadeHero user32)
