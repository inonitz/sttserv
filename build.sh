#!/bin/bash


PROJECT_NAME=all
CMAKE_ARGLIST="\
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
    -DCMAKE_COLOR_DIAGNOSTICS=ON \
    -DFORCE_COLOURED_OUTPUT=ON \
    -DENABLE_SANITIZER_ADDRESS=OFF \
    -DENABLE_SANITIZER_UNDEFINED=OFF \
    -DENABLE_SANITIZER_MEMORY=OFF \
    -DENABLE_LINK_TIME_OPTIMIZATION=OFF \
    -DTREELIB_BUILD_TESTS=OFF \
    -DGGML_VULKAN=ON \
    -DWHISPER_STANDALONE=ON \
    -DWHISPER_ALL_WARNINGS=ON \
    -DWHISPER_ALL_WARNINGS_3RD_PARTY=ON \
    -DWHISPER_FATAL_WARNINGS=OFF \
    -DWHISPER_USE_SYSTEM_GGML=OFF \
    -DWHISPER_SANITIZE_THREAD=${ENABLE_SANITIZER_THREAD} \
    -DWHISPER_SANITIZE_ADDRESS=${ENABLE_SANITIZER_ADDRESS} \
    -DWHISPER_SANITIZE_UNDEFINED=${ENABLE_SANITIZER_UNDEFINED} \
    -DWHISPER_BUILD_TESTS=OFF \
    -DWHISPER_BUILD_EXAMPLES=OFF \
    -DWHISPER_BUILD_SERVER=OFF \
    -DWHISPER_CURL=OFF \
    -DWHISPER_SDL2=ON \
    -DWHISPER_FFMPEG=ON \
    -DWHISPER_COREML=OFF \
    -DWHISPER_COREML_ALLOW_FALLBACK=OFF \
    -DWHISPER_OPENVINO=OFF \
    -DSDL_SHARED=${BUILD_SHARED_LIBS} \
    -DSDL_TEST_LIBRARY=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_DISABLE_INSTALL=ON \
    -DSDL_DISABLE_INSTALL_DOCS=OFF \
    -DSDL_INSTALL_TESTS=OFF \
    -DSDL_AUDIO=ON \
    -DSDL_VIDEO=OFF \
    -DSDL_RENDER=OFF \
    -DSDL_JOYSTICK=OFF \
    -DSDL_HAPTIC=OFF \
    -DSDL_HIDAPI=OFF \
    -DSDL_POWER=OFF \
    -DSDL_FILE=OFF \
    -DSDL_SENSOR=OFF \
    -DSDL_WASAPI=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_EXAMPLES=OFF \

# -DBUILD_GMOCK=OFF \
# -DINSTALL_GTEST=OFF \
# -DBENCHMARK_ENABLE_INSTALL=OFF \
# -DBENCHMARK_INSTALL_DOCS=OFF \
# -DBENCHMARK_INSTALL_TOOLS=OFF \
# -DBENCHMARK_DOWNLOAD_DEPENDENCIES=OFF \
# -DBENCHMARK_ENABLE_TESTING=OFF \
# -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
# -DBENCHMARK_USE_BUNDLED_GTEST=OFF




CMAKE_ORIGINAL_SCRIPT_PATH="$PWD" # Assuming that build.sh is in the same dir as the Root CMakeLists.txt
CMAKE_ROOT_BUILD_DIR="build"
CMAKE_INTRMD_BUILD_DIR=""
CLEAN_CURRENT_ROOT_BUILD_DIR=false
CONFIGURE_CMAKE_FLAG=false
BUILD_BINARIES_FLAG=false
RUN_SANDBOX_FLAG=false
RUN_DEBUG_SANDBOX_FLAG=false

# DEBUG_TEST_SUITE_CXX_FLAG=false
# DEBUG_TEST_SUITE_C_FLAG=false
# RUN_BENCH_FLAG=false
# DEBUG_BENCH_FLAG=false


if [ "$1" = "help" ] || [ "$1" = "--help" ] || [ "$1" = "--h" ]; then
    echo "Usage: $0 <build_type> <library_type> <action>"
    echo ""
    echo "Arguments:"
    echo "  build_type   - Type of build: debug, release, release_dbginfo, debug_perf, release_perf"
    echo "  library_type - Type of library: shared (.dll/.so), static (.lib/.a)"
    echo "  action       - Action to take: cleanbuild, configure, build, sandbox, debugsandbox"
    echo ""
    echo "Options:"
    echo "  help         - Display this help message"
    echo ""
    echo "Examples:"
    echo "  ./build.sh debug   static build"
    echo "  ./build.sh release shared build"
    exit 0
fi




if [ $# -ne 3 ]; then
    echo "3 Arguments required to run the script"
    exit
fi


# argument parsing
if [ $1 = "debug" ];
then
    CMAKE_ARGLIST+=" -DCMAKE_BUILD_TYPE=Debug"
    CMAKE_INTRMD_BUILD_DIR+="debug/"

elif [ $1 = "debug_perf" ];
then
    CMAKE_ARGLIST+=" -DCMAKE_BUILD_TYPE=Debug"
    CMAKE_ARGLIST+=" -DMEASURE_PERFORMANCE_TIMEOUT=1"
    CMAKE_INTRMD_BUILD_DIR+="debug_perf/"

elif [ $1 = "release" ];
then
    CMAKE_ARGLIST+=" -DCMAKE_BUILD_TYPE=Release"
    CMAKE_INTRMD_BUILD_DIR+="release/"

elif [ $1 = "release_dbginfo" ];
then
    CMAKE_ARGLIST+=" -DCMAKE_BUILD_TYPE=RelWithDbgInfo"
    CMAKE_INTRMD_BUILD_DIR+="release_dbginfo/"

elif [ $1 = "release_perf" ];
then
    CMAKE_ARGLIST+=" -DCMAKE_BUILD_TYPE=Release"
    CMAKE_ARGLIST+=" -DMEASURE_PERFORMANCE_TIMEOUT=1"
    CMAKE_INTRMD_BUILD_DIR+="release_perf/"

else
    printf "Unknown Argument %s - valid values are: debug, release, release_dbginfo\nExiting..." $1
    exit
fi


if [ $2 = "shared" ];
then
    CMAKE_ARGLIST+=" -DBUILD_SHARED_LIBS=1"
    CMAKE_INTRMD_BUILD_DIR+="shared/"

elif [ $2 = "static" ];
then
    CMAKE_ARGLIST+=" -DBUILD_SHARED_LIBS=0"
    CMAKE_INTRMD_BUILD_DIR+="static/"

else
    printf "Unknown Argument %s - valid values are: shared, static\nExiting..." $2
    exit
fi



if [ $3 = "cleanbuild" ];
then
    CLEAN_CURRENT_ROOT_BUILD_DIR=true

elif [ $3 = "configure" ];
then
    CONFIGURE_CMAKE_FLAG=true
    CMAKE_ARGLIST+=" -DGIT_SUBMODULE=ON"

elif [ $3 = "build" ];
then
    BUILD_BINARIES_FLAG=true

elif [ $3 = "sandbox" ];
then
    RUN_SANDBOX_FLAG=true

elif [ $3 = "debugsandbox" ];
then
    RUN_DEBUG_SANDBOX_FLAG=true

else
    printf "Unknown Argument %s - valid values are: cleanbuild, configure, build, test, debugcxxtests, debugctests, benchmark, debugbenchmark\nExiting..." $3
    exit
fi





# the actual script
CMAKE_FINAL_BUILD_DIR="$CMAKE_ROOT_BUILD_DIR/$CMAKE_INTRMD_BUILD_DIR"
echo "Out-of-source Root   Build Directory is '$CMAKE_ROOT_BUILD_DIR' "
echo "Out-of-source Target Build Directory is '$CMAKE_FINAL_BUILD_DIR' "
echo "Cmake Arguments passed are ==> { "
echo "$CMAKE_ARGLIST"
echo "}"
echo "Script arguments are '$1' '$2' '$3' "


mkdir -p build
if [ "$CLEAN_CURRENT_ROOT_BUILD_DIR" = "true" ];
then
    rm -rf $CMAKE_FINAL_BUILD_DIR
fi


if [ "$CONFIGURE_CMAKE_FLAG" = "true" ]
then
    mkdir -p $CMAKE_FINAL_BUILD_DIR
    cmake -S . -B $CMAKE_FINAL_BUILD_DIR -G 'Ninja' $CMAKE_ARGLIST
fi


if [ "$BUILD_BINARIES_FLAG" = "true" ];
then
    cd $CMAKE_FINAL_BUILD_DIR
    cp "compile_commands.json" "../../compile_commands.json" 
    # echo "CURRENT WORKING DIRECTORY IS $PWD"
    ninja $PROJECT_NAME -j$(( $(nproc) / 2 > 0 ? $(nproc) / 2 : 1 ))
fi


# Running any of these targets Assumes that we've already built it.
# Otherwise, we need to run the configure & build options beforehand.
if [ $RUN_SANDBOX_FLAG = "true" ];
then
    cd $CMAKE_FINAL_BUILD_DIR
    ninja run_sandbox
fi

if [ $RUN_DEBUG_SANDBOX_FLAG = "true" ];
then
    cd $CMAKE_FINAL_BUILD_DIR
    ninja debug_sandbox
fi




# cd ../ # leave static/shared
# cd ../ # leave debug/release/etc...
# cd ../ # build