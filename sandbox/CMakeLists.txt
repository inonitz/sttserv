cmake_minimum_required(VERSION 3.16)
project(sttserver_sandbox C CXX)


set(SOURCES
    source/async_key.cpp
    source/audio.cpp
    source/miniaudio_impl.c
    source/key_codes.cpp
    source/whisper_init.cpp
    source/examples/resample_linux.cpp
)

set(HEADERS

    include/sandbox/key_codes.hpp
    include/sandbox/async_key.hpp
    include/sandbox/audio.hpp
    include/sandbox/whisper_init.hpp
)


add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${HEADERS})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(${PROJECT_NAME} INTERFACE 
    c_std_11
    cxx_std_17
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    miniaudio
    whisper
    UTIL2::util2
    cxxopts::cxxopts
    # WORKSPACE_CONFIG::ProjectDefaultConfig
)




add_custom_target(run_${PROJECT_NAME}
    COMMAND ./${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${WORKSPACE_GLOBAL_CUSTOM_BUILD_OUTPUT_DIRECTORY}
    USES_TERMINAL
    COMMENT "Running Sandbox..."
)

add_custom_target(debug_${PROJECT_NAME}
    COMMAND ./${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${WORKSPACE_GLOBAL_CUSTOM_BUILD_OUTPUT_DIRECTORY}
    USES_TERMINAL
    COMMENT "Running Debugging Sandbox..."
)