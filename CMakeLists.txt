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

OPTION(TREELIB_BUILD_TESTS
    "Create Build, Run Targets for Testing of the whole project"
    OFF
)

OPTION(FORCE_COLOURED_OUTPUT
    "Always produce ANSI-colored output (GNU/Clang only)"
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


add_subdirectory(dependencies/util2)
add_subdirectory(dependencies/whispercpp)
add_subdirectory(dependencies/miniaudio)
add_subdirectory(sandbox)
# add_subdirectory(sttserv)
# add_subdirectory(benchmark)

