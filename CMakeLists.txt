cmake_minimum_required(VERSION 3.28)

project(enigma
  VERSION 0.0.0
  DESCRIPTION "C library: enigma"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(enigma_BUILD_APP   "Build demo console app (src/main.c)" ON)
option(enigma_BUILD_TESTS "Build tests in /tests"               ON)
option(BUILD_SHARED_LIBS           "Build shared instead of static"      OFF)

include(GNUInstallDirs)

# -------------------- Dependencies (discovery + selection) --------------------
# Choose how this library obtains its dependencies.
option(enigma_USE_BUNDLED_SUBMODULES "Use submodules from external/*" OFF)

# --- Cache definitions that Projects depend on ---
# set(NEXUS_DOUBLE_PRECISION ON CACHE BOOL "" FORCE)

# Accumulate deps here; apply them after add_library()
set(enigma_PUBLIC_DEPS  "")  # e.g. nexus::nexus
set(enigma_PRIVATE_DEPS "")  # e.g. chaos::chaos

if(enigma_USE_BUNDLED_SUBMODULES)
  # Example: disable submodule demos/tests
  # set(nexus_BUILD_APP   OFF CACHE BOOL "" FORCE)
  # set(nexus_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  # set(chaos_BUILD_APP   OFF CACHE BOOL "" FORCE)
  # set(chaos_BUILD_TESTS OFF CACHE BOOL "" FORCE)

  # Bring sources into this build (order matters if they depend on each other)
  # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/nexus)
  # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/chaos)
else()
  # Use installed packages (or targets provided by a parent project)
   if(NOT TARGET nexus::nexus)
     find_package(nexus CONFIG REQUIRED) # optionally: find_package(nexus 1.2 CONFIG REQUIRED)
   endif()
  # if(NOT TARGET chaos::chaos)
  #   find_package(chaos CONFIG REQUIRED)
  # endif()
endif()

# Decide visibility (examples):
list(APPEND enigma_PUBLIC_DEPS  nexus::nexus)  # appears in public headers
# list(APPEND enigma_PRIVATE_DEPS chaos::chaos)  # used only in .c files

# ------------------------------------------------------------------------------

# ---- Sources (recursively, exclude app entry from the lib) ----
file(GLOB_RECURSE enigma_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
list(FILTER enigma_SOURCES EXCLUDE REGEX "/main\\.c$")

# ---- Library ----
add_library(enigma ${enigma_SOURCES})
add_library(enigma::enigma ALIAS enigma)

set_target_properties(enigma PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION   0.0.0
  SOVERSION 0
)

target_include_directories(enigma
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ---- Apply dependency link interfaces (MUST be after add_library) ----
# Safe even if the lists are empty.
target_link_libraries(enigma
  PUBLIC  ${enigma_PUBLIC_DEPS}
  PRIVATE ${enigma_PRIVATE_DEPS}
)

# (Optional) warnings on MSVC/clang-cl
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  target_compile_options(enigma PRIVATE /W4)
elseif (MSVC)
  target_compile_options(enigma PRIVATE /W4)
endif()

# ---- Demo app (optional) ----
if(enigma_BUILD_APP)
  add_executable(enigma_app src/main.c)
  target_link_libraries(enigma_app PRIVATE enigma::enigma)
endif()

# ---- Tests (optional & minimal) ----
if(enigma_BUILD_TESTS)
  enable_testing()
  add_executable(enigma_tests tests/entry.c)
  target_link_libraries(enigma_tests PRIVATE enigma::enigma)
  add_test(NAME enigma.basic COMMAND enigma_tests)
endif()

# ---- Install & export (find_package support) ----
include(CMakePackageConfigHelpers)

install(TARGETS enigma
  EXPORT enigmaTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/enigma-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/enigma-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/enigma
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/enigma-config-version.cmake
  VERSION 0.0.0
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/enigma-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/enigma-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/enigma
)

install(EXPORT enigmaTargets
  NAMESPACE enigma::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/enigma
)

# Build-tree export for add_subdirectory() consumers
export(EXPORT enigmaTargets
  NAMESPACE enigma::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/enigmaTargets.cmake
)
