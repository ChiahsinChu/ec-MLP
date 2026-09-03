# ##############################################################################
# CMake build system for the constant-potential DPLR fixes.
#
# This builds a standalone LAMMPS plugin, separate from the sibling one in
# ../verlet-split-dplr. It has no deepmd-kit dependency: fix electrode/conp/dplr
# and fix electrode/conq/dplr derive from the LAMMPS ELECTRODE package and touch
# nothing else. The kspace style they run with, pppm/electrode/dplr, is added
# to deepmd-kit itself by patch/202404-fix_dplr-lammps-*.patch.
# ##############################################################################

cmake_minimum_required(VERSION 3.10)

# enforce out-of-source build
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(
    FATAL_ERROR
      "In-source builds are not allowed. You must create and use a build directory. "
      "Please remove CMakeCache.txt and CMakeFiles first.")
endif()

project(
  ecmlp_legacy_plugin
  VERSION 1.0
  LANGUAGES CXX)

# when this file is included as subdirectory in the LAMMPS build, many settings
# are directly imported
if(LAMMPS_DIR)
  set(LAMMPS_HEADER_DIR ${LAMMPS_SOURCE_DIR})
else()
  # NOTE: the next line should be commented out when used outside of the LAMMPS
  # package get_filename_component(LAMMPS_SOURCE_DIR
  # ${PROJECT_SOURCE_DIR}/../../src ABSOLUTE)
  set(LAMMPS_HEADER_DIR
      ${LAMMPS_SOURCE_DIR}
      CACHE PATH "Location of LAMMPS headers")
  if(NOT LAMMPS_HEADER_DIR)
    message(FATAL_ERROR "Must set LAMMPS_HEADER_DIR")
  endif()
  # by default, install into $HOME/.local (not /usr/local), so that no root
  # access (and sudo) is needed
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
        "$ENV{HOME}/.local"
        CACHE PATH "Default install path" FORCE)
  endif()
  # ugly hacks for MSVC which by default always reports an old C++ standard in
  # the __cplusplus macro and prints lots of pointless warnings about "unsafe"
  # functions
  if(MSVC)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      add_compile_options(/Zc:__cplusplus)
      add_compile_options(/wd4244)
      add_compile_options(/wd4267)
      if(LAMMPS_EXCEPTIONS)
        add_compile_options(/EHsc)
      endif()
    endif()
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  endif()
endif()

# C++17 is required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Need -restrict with Intel compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -restrict")
endif()

# LAMMPSInterfaceCXX.cmake is shared with the sibling plugin, one level up.
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
include(CheckIncludeFileCXX)
if(NOT LAMMPS_DIR)
  include(LAMMPSInterfaceCXX)
endif()

include(${LAMMPS_SOURCE_DIR}/../cmake/Modules/LAMMPSUtils.cmake)
get_lammps_version(${LAMMPS_SOURCE_DIR}/version.h LAMMPS_VERSION_NUMBER)

# Debug logging to validate version number format
message(STATUS "Original LAMMPS_VERSION_NUMBER: ${LAMMPS_VERSION_NUMBER}")

# Fix version number with multiple decimal points by converting to a valid C++
# number Replace dots with nothing to create a numeric version like 2025072222
string(REPLACE "." "" LAMMPS_VERSION_NUMBER_FIXED ${LAMMPS_VERSION_NUMBER})
message(STATUS "Fixed LAMMPS_VERSION_NUMBER: ${LAMMPS_VERSION_NUMBER_FIXED}")

# ec-MLP supports LAMMPS stable 22 Jul 2025 and newer. Before that release
# FixElectrodeConp::recompute_potential() took its two arguments by value rather
# than by const reference, so the conq body copied into
# fix_electrode_conq_dplr.cpp would not override anything and would silently
# never be called. Fail loudly instead.
#
# get_lammps_version() emits "year.month.day.update" with no leading zeros in
# recent LAMMPS and a plain YYYYMMDD in older ones. Neither the stripped string
# nor the dotted one compares as a date, so rebuild one from the parts.
if(LAMMPS_VERSION_NUMBER MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
  math(EXPR LAMMPS_VERSION_DATE
       "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")
else()
  set(LAMMPS_VERSION_DATE ${LAMMPS_VERSION_NUMBER})
endif()
if(LAMMPS_VERSION_DATE LESS 20250722)
  message(
    FATAL_ERROR
      "LAMMPS ${LAMMPS_VERSION_NUMBER} is too old for these styles; the stable "
      "22 Jul 2025 release or newer is required.")
endif()

# Both styles here derive from FixElectrodeConp, so unlike ../lmp the ELECTRODE
# package is not optional.
if(NOT EXISTS ${LAMMPS_SOURCE_DIR}/ELECTRODE/fix_electrode_conp.h)
  message(
    FATAL_ERROR
      "The LAMMPS ELECTRODE package was not found in ${LAMMPS_SOURCE_DIR}. "
      "fix electrode/conp/dplr and fix electrode/conq/dplr derive from it, so "
      "LAMMPS must be built with -D PKG_ELECTRODE=yes before this plugin can "
      "be compiled.")
endif()

set(libname "ecmlplegacyplugin")
file(GLOB LMP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(${libname} MODULE ${LMP_SRC})
target_link_libraries(${libname} PRIVATE lammps)
target_include_directories(
  ${libname}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/..
          ${LAMMPS_SOURCE_DIR}
          ${LAMMPS_SOURCE_DIR}/PLUGIN
          ${LAMMPS_SOURCE_DIR}/KSPACE
          ${LAMMPS_SOURCE_DIR}/ELECTRODE)
set_target_properties(${libname} PROPERTIES PREFIX "" SUFFIX ".so")
target_compile_definitions(
  ${libname} PRIVATE "LAMMPS_VERSION_NUMBER=${LAMMPS_VERSION_NUMBER_FIXED}")
message("${LAMMPS_SOURCE_DIR}")

# ##############################################################################
# building the plugins

# MacOS seems to need this
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set_target_properties(${libname} PROPERTIES LINK_FLAGS
                                              "-Wl,-undefined,dynamic_lookup")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # tell CMake to export all symbols to a .dll on Windows with special case for
  # MinGW cross-compilers
  set_target_properties(${libname} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
  if(CMAKE_CROSSCOMPILING)
    set_target_properties(${libname} PROPERTIES LINK_FLAGS
                                                "-Wl,--export-all-symbols")
  endif()
else()
  set_target_properties(${libname} PROPERTIES LINK_FLAGS "-rdynamic")
endif()

add_custom_target(
  plugins ALL
  ${CMAKE_COMMAND} -E echo "Building Plugins"
  DEPENDS ${libname})
