cmake_minimum_required(VERSION 3.16)
project(self_tuning_lam_annealing VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_library(stlam INTERFACE)
add_library(stlam::stlam ALIAS stlam)

target_include_directories(stlam INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(stlam INTERFACE cxx_std_17)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  option(STLAM_BUILD_EXAMPLES "Build examples" ON)
else()
  option(STLAM_BUILD_EXAMPLES "Build examples" OFF)
endif()

add_library(stlam_dev_flags INTERFACE)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Warnings
  target_compile_options(stlam_dev_flags INTERFACE -Wall -Wextra -Werror -pedantic-errors)

  # Sanitize
  target_compile_options(stlam_dev_flags INTERFACE -g -D_GLIBCXX_DEBUG
    -fno-omit-frame-pointer -fno-sanitize-recover=all -fsanitize=address,undefined)
  target_link_options(stlam_dev_flags INTERFACE -fsanitize=address,undefined)
endif()

if(STLAM_BUILD_EXAMPLES)
  add_executable(onemax examples/onemax.cc)
  target_link_libraries(onemax PRIVATE stlam::stlam stlam_dev_flags)
endif()
