cmake_minimum_required(VERSION 3.16)
project(SimpleBATConverter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "Simple BAT Converter targets Windows only.")
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ---------------------------------------------------------------------------
# Launcher stubs (built first, then embedded into sbc)
# ---------------------------------------------------------------------------
add_executable(launcher_console
  resources/launcher/launcher_main.cpp
)
target_compile_definitions(launcher_console PRIVATE SBC_CONSOLE=1)
target_link_libraries(launcher_console PRIVATE ole32 shell32)
set_target_properties(launcher_console PROPERTIES
  OUTPUT_NAME "launcher_console"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_RelWithDebInfo "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_MinSizeRel "${CMAKE_BINARY_DIR}/stubs"
)
if(MSVC)
  target_link_options(launcher_console PRIVATE /SUBSYSTEM:CONSOLE)
else()
  target_link_options(launcher_console PRIVATE -mconsole -municode)
endif()

add_executable(launcher_noconsole
  resources/launcher/launcher_main.cpp
)
target_compile_definitions(launcher_noconsole PRIVATE SBC_CONSOLE=0)
target_link_libraries(launcher_noconsole PRIVATE ole32 shell32)
set_target_properties(launcher_noconsole PROPERTIES
  OUTPUT_NAME "launcher_noconsole"
  WIN32_EXECUTABLE TRUE
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_RelWithDebInfo "${CMAKE_BINARY_DIR}/stubs"
  RUNTIME_OUTPUT_DIRECTORY_MinSizeRel "${CMAKE_BINARY_DIR}/stubs"
)
if(MSVC)
  target_link_options(launcher_noconsole PRIVATE /SUBSYSTEM:WINDOWS)
else()
  target_link_options(launcher_noconsole PRIVATE -mwindows -municode)
endif()

# ---------------------------------------------------------------------------
# Embed stub binaries as C++ headers
# ---------------------------------------------------------------------------
set(STUB_CONSOLE_BIN "${CMAKE_BINARY_DIR}/stubs/launcher_console.exe")
set(STUB_NOCONSOLE_BIN "${CMAKE_BINARY_DIR}/stubs/launcher_noconsole.exe")
set(EMBED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${EMBED_DIR}")
set(EMBED_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/embed_stub.cmake")

add_custom_command(
  OUTPUT "${EMBED_DIR}/stub_console.hpp"
  COMMAND ${CMAKE_COMMAND}
          -DINPUT_FILE=${STUB_CONSOLE_BIN}
          -DOUTPUT_FILE=${EMBED_DIR}/stub_console.hpp
          -DVARIABLE_NAME=kStubConsole
          -P ${EMBED_SCRIPT}
  DEPENDS launcher_console "${EMBED_SCRIPT}"
  COMMENT "Embedding console launcher stub"
  VERBATIM
)

add_custom_command(
  OUTPUT "${EMBED_DIR}/stub_noconsole.hpp"
  COMMAND ${CMAKE_COMMAND}
          -DINPUT_FILE=${STUB_NOCONSOLE_BIN}
          -DOUTPUT_FILE=${EMBED_DIR}/stub_noconsole.hpp
          -DVARIABLE_NAME=kStubNoConsole
          -P ${EMBED_SCRIPT}
  DEPENDS launcher_noconsole "${EMBED_SCRIPT}"
  COMMENT "Embedding no-console launcher stub"
  VERBATIM
)

add_custom_target(embed_stubs DEPENDS
  "${EMBED_DIR}/stub_console.hpp"
  "${EMBED_DIR}/stub_noconsole.hpp"
)

# ---------------------------------------------------------------------------
# Main tool: sbc.exe
# ---------------------------------------------------------------------------
set(SBC_SOURCES
  src/main.cpp
  src/cli.cpp
  src/compiler.cpp
  src/file_utils.cpp
  src/icon.cpp
  src/version.cpp
)

if(MSVC)
  enable_language(RC)
  list(APPEND SBC_SOURCES resources/sbc.rc)
endif()

add_executable(sbc ${SBC_SOURCES})
target_include_directories(sbc PRIVATE include "${EMBED_DIR}")
target_link_libraries(sbc PRIVATE shlwapi)
add_dependencies(sbc embed_stubs)

if(NOT MSVC)
  target_link_options(sbc PRIVATE -mconsole -municode)
endif()

set_target_properties(sbc PROPERTIES
  OUTPUT_NAME "sbc"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RelWithDebInfo "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_MinSizeRel "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS sbc RUNTIME DESTINATION bin)

message(STATUS "Simple BAT Converter ${PROJECT_VERSION}")
