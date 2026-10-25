cmake_minimum_required(VERSION 3.16)
project(self_tuning_lam_annealing VERSION 0.0.0 LANGUAGES CXX)

# Disable compiler-specific extensions.
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile command for clangd and other tools.
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Header-only library target and namespaced alias.
add_library(stlam INTERFACE)
add_library(stlam::stlam ALIAS stlam)

# Public include paths for build and install trees.
target_include_directories(stlam INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Require C++17.
target_compile_features(stlam INTERFACE cxx_std_17)

# Developer-only flags: warnings and sanitizers.
add_library(stlam_dev_flags INTERFACE)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Warnings
  target_compile_options(stlam_dev_flags INTERFACE -Wall -Wextra -Werror -pedantic-errors)

  # ASan and UBSan
  target_compile_options(stlam_dev_flags INTERFACE -g -D_GLIBCXX_DEBUG
    -fno-omit-frame-pointer -fno-sanitize-recover=all -fsanitize=address,undefined)
  target_link_options(stlam_dev_flags INTERFACE -fsanitize=address,undefined)
endif()

# Build examples by default when this is the top-level project.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  option(STLAM_BUILD_EXAMPLES "Build examples" ON)
else()
  option(STLAM_BUILD_EXAMPLES "Build examples" OFF)
endif()

# Example executables.
if(STLAM_BUILD_EXAMPLES)
  add_executable(onemax_copy examples/onemax/onemax_copy.cc)
  add_executable(onemax_delta examples/onemax/onemax_delta.cc)
  target_link_libraries(onemax_copy PRIVATE stlam::stlam stlam_dev_flags)
  target_link_libraries(onemax_delta PRIVATE stlam::stlam stlam_dev_flags)
endif()

# Optional Doxygen documentation target.
option(STLAM_BUILD_DOCS "Build Doxygen documentation" OFF)
if(STLAM_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/doxygen/Doxyfile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    @ONLY
  )
  add_custom_target(stlam_docs
    COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating Doxygen HTML"
    VERBATIM
  )
endif()
