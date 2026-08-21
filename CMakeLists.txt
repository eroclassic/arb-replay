cmake_minimum_required(VERSION 3.20)
project(ArbReplay VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARBREPLAY_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(arbreplay_project_options INTERFACE)
target_compile_features(arbreplay_project_options INTERFACE cxx_std_20)
target_include_directories(arbreplay_project_options INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

if(MSVC)
  target_compile_options(arbreplay_project_options INTERFACE /W4 /permissive-)
else()
  target_compile_options(arbreplay_project_options INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)

  if(ARBREPLAY_ENABLE_SANITIZERS)
    target_compile_options(arbreplay_project_options INTERFACE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(arbreplay_project_options INTERFACE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

