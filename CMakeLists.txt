cmake_minimum_required(VERSION 3.16)
project(sttserver_workspace C CXX)

include(SubmoduleUpdate.cmake)
include(UseCCache.cmake)
include(OutputDir.cmake)
include(WorkspaceOptions.cmake)


# Workspace/Project-Wide Specific Configurations
update_submodules_on_cmake_configure("dependencies")
use_ccache_program_if_exists()
define_aggregate_output_artifact_directory()


# Workspace/Project-Wide Options
option(ENABLE_SANITIZER_ADDRESS
    "Enable AddressSanitizer"
    OFF
)

option(ENABLE_SANITIZER_UNDEFINED
    "Enable UndefinedBehaviorSanitizer"
    OFF
)

option(ENABLE_SANITIZER_MEMORY
    "Enable MemorySanitizer"
    OFF
)

option(ENABLE_LINK_TIME_OPTIMIZATION
    "Enable Link-Time/Interprocedural Optimization (LTO/IPO)"
    OFF
)

OPTION(BUILD_TESTS
    "Create Build, Run Targets for Testing of the whole project"
    OFF
)

OPTION(FORCE_COLOURED_OUTPUT
    "Always produce ANSI-colored output (GNU/Clang only)"
    ON
)

option(ENABLE_PROFILING
    "Enable Usage &&|| Code Paths with the MicroProfile profiler" 
    ON
)


if(ENABLE_SANITIZER_ADDRESS AND ENABLE_SANITIZER_MEMORY)
    message(FATAL_ERROR "ASan and MSan are mutually exclusive. Please choose only one\n")
endif()

# Thanks to the following gentlemen:
# https://stackoverflow.com/a/77419222
# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
if(FORCE_COLOURED_OUTPUT)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(-fansi-escape-codes)
        add_compile_options(-fcolor-diagnostics)
    endif()
endif()


# Declare interface libraries 
declare_optional_interface_library_different_sanitizers()
declare_optional_interface_library_lto()
declare_optional_interface_default_project_configuration()


add_subdirectory(dependencies/cxxopts)
add_subdirectory(dependencies/miniaudio)
add_subdirectory(dependencies/util2)
add_subdirectory(dependencies/whispercpp)
if(ENABLE_PROFILING)
    message(STATUS "Building Tracy for Profiling!")
    add_subdirectory(dependencies/tracy)
endif()

add_subdirectory(sandbox)
# add_subdirectory(sttserv)
# add_subdirectory(benchmark)



# if(TRACY_ENABLE)
#     add_subdirectory(dependencies/tracy)

#     ExternalProject_Add(tracy_profiler_build
#         SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tracy/profiler"
#         CMAKE_ARGS 
#             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#             -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
#             -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
#             -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
#             -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
#             -DNO_ISA_EXTENSIONS=ON
#             "-DCMAKE_C_FLAGS=-D_CRT_SECURE_NO_WARNINGS=1 -DTRACY_NO_CAPSTONE=1"
#             "-DCMAKE_CXX_FLAGS=-D_CRT_SECURE_NO_WARNINGS=1 -DTRACY_NO_CAPSTONE=1"
#             # Inject overrides into the build-type variables so they appear last
#             "-DCMAKE_C_FLAGS_RELEASE=-Wno-error -Wno-deprecated-declarations -Wno-unused-function"
#             "-DCMAKE_CXX_FLAGS_RELEASE=-Wno-error"
#             "-DCMAKE_C_FLAGS_DEBUG=-Wno-error -Wno-deprecated-declarations -Wno-unused-function"
#             "-DCMAKE_CXX_FLAGS_DEBUG=-Wno-error"
#             "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-Wno-error -Wno-deprecated-declarations -Wno-unused-function"
#             "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-Wno-error"
#         INSTALL_COMMAND ""
#         BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/tracy_profiler_bin"
#         USES_TERMINAL_CONFIGURE 1
#         USES_TERMINAL_BUILD 1
#     )

#     ExternalProject_Add(tracy_capture_build
#         SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tracy/capture"
#         CMAKE_ARGS 
#             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#             -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
#             -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
#             -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
#             -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
#             -DNO_ISA_EXTENSIONS=ON
#             "-DCMAKE_C_FLAGS=-D_CRT_SECURE_NO_WARNINGS=1 -DTRACY_NO_CAPSTONE=1"
#             "-DCMAKE_CXX_FLAGS=-D_CRT_SECURE_NO_WARNINGS=1 -DTRACY_NO_CAPSTONE=1"
#             # Inject overrides into the build-type variables so they appear last
#             "-DCMAKE_C_FLAGS_RELEASE=-Wno-error -Wno-deprecated-declarations -Wno-unused-function"
#             "-DCMAKE_CXX_FLAGS_RELEASE=-Wno-error"
#             "-DCMAKE_C_FLAGS_DEBUG=-Wno-error -Wno-deprecated-declarations -Wno-unused-function"
#             "-DCMAKE_CXX_FLAGS_DEBUG=-Wno-error"
#             "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-Wno-error -Wno-deprecated-declarations -Wno-unused-function"
#             "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-Wno-error"

#         INSTALL_COMMAND ""
#         BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/tracy_capture_bin"
#         USES_TERMINAL_CONFIGURE 1
#         USES_TERMINAL_BUILD 1
#     )

# endif()

