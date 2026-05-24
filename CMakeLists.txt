cmake_minimum_required(VERSION 3.16)
project(sttserver_workspace C CXX)

include(SubmoduleUpdate.cmake)
include(UseCCache.cmake)
include(ColouredOutput.cmake)
include(OutputDir.cmake)
include(WorkspaceOptions.cmake)
include(BuildDiagnostics.cmake)

# 1. Workspace/Project-Wide Specific Configurations
enable_coloured_diagnostics_if_available()
use_ccache_program_if_exists()
define_aggregate_output_artifact_directory()


update_submodules_on_cmake_configure("dependencies")


# 2. Workspace/Project-Wide Options
OPTION(STTSERVER_FORCE_COLOURED_OUTPUT         "Always produce ANSI-colored output (GNU/Clang only) (Platform Agnostic)"                                                                                ON)
OPTION(STTSERVER_ENABLE_SANITIZER_ADDRESS      "Enable AddressSanitizer"                                                                                                                                OFF)
OPTION(STTSERVER_ENABLE_SANITIZER_UNDEFINED    "Enable UndefinedBehaviorSanitizer"                                                                                                                      OFF)
OPTION(STTSERVER_ENABLE_SANITIZER_MEMORY       "Enable MemorySanitizer"                                                                                                                                 OFF)
OPTION(STTSERVER_ENABLE_LINK_TIME_OPTIMIZATION "Enable Link-Time/Interprocedural Optimization (LTO/IPO)"                                                                                                OFF)
OPTION(STTSERVER_ENABLE_TRACY_PROFILING        "Enable Code-paths for the Tracy Profiler"                                                                                                               OFF)
OPTION(STTSERVER_BUILD_BACKEND_WHISPER         "Build the server with the master branch of https://github.com/ggml-org/whisper.cpp (GGML Runtime - CPU OR Modern Cuda OR Modern ROCm OR Vulkan OR Metal)" OFF)
OPTION(STTSERVER_BUILD_BACKEND_PARAKEET        "Build the server with the feature-parakeet branch of https://github.com/danbev/whisper.cpp/tree/parakeet-support (GGML Runtime)"                        OFF)
OPTION(STTSERVER_BUILD_BACKEND_SHERPA_ONNX     "Build the server with the sherpa-onnx (Onnxruntime - CPU OR Modern Cuda/Modern ROCm)"                                                                   OFF)
OPTION(STTSERVER_BUILD_EXECUTABLE              "Build The actual Speech-To-Text [ASR] Server. use sttserv --help for options"                                                                           OFF)
OPTION(STTSERVER_BUILD_TESTS                   "Testing Correct functionality of the project"                                                                                                           OFF)
OPTION(STTSERVER_BUILD_BENCHMARKS              "Testing Performance/Accuracy Metrics The project needs to achieve"                                                                                      OFF)

# Early error conditions
if(ENABLE_SANITIZER_ADDRESS AND ENABLE_SANITIZER_MEMORY)
    message(FATAL_ERROR "ASan and MSan are mutually exclusive. Please choose only one\n")
endif()

if(NOT STTSERVER_BUILD_BACKEND_WHISPER AND NOT STTSERVER_BUILD_BACKEND_PARAKEET AND NOT STTSERVER_BUILD_BACKEND_SHERPA_ONNX)
    message(FATAL_ERROR "Must Select Atleast 1 Backend to build the project with")
endif()

if(STTSERVER_BUILD_BACKEND_WHISPER AND STTSERVER_BUILD_BACKEND_PARAKEET)
    message(FATAL_ERROR "Compiling with Both Libraries will create multiple definitions of the same underlying library. Enable one or the other.")
endif()


# 3. Declare interface libraries - Sanitizer Interface Library depends on these
declare_optional_interface_library_different_sanitizers()
declare_optional_interface_library_lto()
declare_optional_interface_default_project_configuration()


# 4. Process Actions related to the options above.
if(STTSERVER_FORCE_COLOURED_OUTPUT)
    set(CMAKE_COLOR_DIAGNOSTICS ON)
    enable_coloured_diagnostics_if_available()
endif()

if(STTSERVER_ENABLE_SANITIZER_ADDRESS)
    add_link_options(WORKSPACE_CONFIG::AddressSanitizerInterface)
endif()

if(STTSERVER_ENABLE_SANITIZER_UNDEFINED)
    add_link_options(WORKSPACE_CONFIG::UBSanInterface)
endif()

if(STTSERVER_ENABLE_SANITIZER_MEMORY)
    add_link_options(WORKSPACE_CONFIG::MemorySanitizerInterface)
endif()

if(STTSERVER_ENABLE_LINK_TIME_OPTIMIZATION)
    add_link_options(WORKSPACE_CONFIG::LTO)
endif()



# 5. Print Enabled/Disabled Flags For Verbose Diagnostics
#       Default flags set here before printing to catch anything wrong
if(STTSERVER_BUILD_BACKEND_WHISPER OR STTSERVER_BUILD_BACKEND_PARAKEET)
    setup_whisper_backend_defaults()
endif()
if(STTSERVER_BUILD_BACKEND_SHERPA_ONNX)
    setup_sherpa_onnx_backend_defaults()
endif()
print_all_workspace_flags_and_vars()


# 6.
# ---------------- The Actual Start of the cmake (after all the configuration bullshit) ----------------
if(STTSERVER_ENABLE_TRACY_PROFILING)
    message(STATUS "Building with Tracy for Profiling!")
    set(TRACY_ENABLE  ON)
    set(TRACY_VERBOSE ON)
    add_subdirectory(dependencies/tracy)
endif()

if(STTSERVER_BUILD_BACKEND_WHISPER)
    message(STATUS "Building with backend whisper.cpp (master branch)")
    add_subdirectory(dependencies/whispercpp)
endif()

if(STTSERVER_BUILD_BACKEND_PARAKEET)
    message(STATUS "Building with backend whisper.cpp (parakeet feature-branch [dan-bev])")
    add_subdirectory(dependencies/parakeetcpp)
endif()

if(STTSERVER_BUILD_BACKEND_SHERPA_ONNX)
    message(STATUS "Building with backend sherpa-onnx")
    add_subdirectory(dependencies/sherpa-onnx)
endif()


add_subdirectory(dependencies/cxxopts)
add_subdirectory(dependencies/miniaudio)
add_subdirectory(dependencies/util2)
if(STTSERVER_BUILD_EXECUTABLE)
    add_subdirectory(sandbox)
endif()
if(STTSERVER_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(STTSERVER_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()



