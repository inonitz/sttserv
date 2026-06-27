cmake_minimum_required(VERSION 3.16)
project(sttserver_workspace C CXX)

# Top-Level Project Check for CMake 3.16 Compatibility
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(STTSERVER_IS_TOP_LEVEL ON)
else()
    set(STTSERVER_IS_TOP_LEVEL OFF)
endif()

include(cmake/FetchCPM.cmake)
include(cmake/SubmoduleUpdate.cmake)
include(cmake/UseCCache.cmake)
include(cmake/ColouredOutput.cmake)
include(cmake/OutputDir.cmake)
include(cmake/WorkspaceOptions.cmake)
include(cmake/BuildDiagnostics.cmake)
include_cpm()


# 1. Workspace/Project-Wide Specific Configurations
enable_coloured_diagnostics_if_available()
use_ccache_program_if_exists()
define_aggregate_output_artifact_directory()
update_submodules_on_cmake_configure("dependencies")


# 2. Workspace/Project-Wide Options
OPTION(STTSERVER_ENABLE_SANITIZER_ADDRESS          "Enable AddressSanitizer"                                                            OFF)
OPTION(STTSERVER_ENABLE_SANITIZER_UNDEFINED        "Enable UndefinedBehaviorSanitizer"                                                  OFF)
OPTION(STTSERVER_ENABLE_SANITIZER_MEMORY           "Enable MemorySanitizer"                                                             OFF)
OPTION(STTSERVER_ENABLE_LINK_TIME_OPTIMIZATION     "Enable Link-Time/Interprocedural Optimization (LTO/IPO)"                            OFF)
OPTION(STTSERVER_ENABLE_TRACY_PROFILING            "Enable Code-paths for the Tracy Profiler"                                           OFF)
OPTION(STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER     "Build the server with the master branch of https://github.com/ggml-org/whisper.cpp" OFF)
OPTION(STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX "Build the server with the sherpa-onnx"                                              OFF)
OPTION(STTSERVER_BUILD_LIBRARY                     "Build The actual Speech-To-Text [ASR] Server"                                       OFF)

# Automatically toggle OFF when included as a submodule
OPTION(STTSERVER_BUILD_TESTS                   "Testing Correct functionality of the project"                      ${STTSERVER_IS_TOP_LEVEL})
OPTION(STTSERVER_BUILD_BENCHMARKS              "Testing Performance/Accuracy Metrics The project needs to achieve" ${STTSERVER_IS_TOP_LEVEL})

# Early error conditions
if(STTSERVER_ENABLE_SANITIZER_ADDRESS AND STTSERVER_ENABLE_SANITIZER_MEMORY)
    message(FATAL_ERROR "ASan and MSan are mutually exclusive. Please choose only one\n")
endif()

if(NOT STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER AND NOT STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX)
    message(FATAL_ERROR "Must Select Atleast 1 Backend to build the project with")
endif()


# 3. Declare interface libraries - Sanitizer Interface Library depends on these
declare_optional_interface_library_different_sanitizers()
declare_optional_interface_library_lto()
declare_optional_interface_default_project_configuration()
enable_coloured_diagnostics_if_available()


# 5. Print Enabled/Disabled Flags For Verbose Diagnostics
if(STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER)
    setup_whisper_backend_defaults()
endif()

if(STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX)
    setup_sherpa_onnx_backend_defaults()
endif()

print_all_workspace_flags_and_vars()

# 6. Actual Start
if(STTSERVER_ENABLE_TRACY_PROFILING)
    safe_cpm_add_package(NAME tracy SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tracy")
endif()
if(STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER)
    safe_cpm_add_package(NAME whispercpp SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/whispercpp")
endif()
if(STTSERVER_BUILD_LIBRARY_BACKEND_PARAKEET)
    safe_cpm_add_package(NAME parakeetcpp SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/parakeetcpp")
endif()
if(STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX)
    safe_cpm_add_package(NAME sherpa-onnx SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/sherpa-onnx")
endif()

safe_cpm_add_package(NAME cxxopts   SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cxxopts")
safe_cpm_add_package(NAME miniaudio SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/miniaudio")
safe_cpm_add_package(NAME util2     SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/util2")


if(STTSERVER_BUILD_LIBRARY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sttserv")
    add_subdirectory(sttserv)
endif()

if(STTSERVER_BUILD_TESTS)
    safe_cpm_add_package(NAME googletest SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/googletest")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
        add_subdirectory(test)
    endif()
endif()

if(STTSERVER_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark")
    add_subdirectory(benchmark)
endif()