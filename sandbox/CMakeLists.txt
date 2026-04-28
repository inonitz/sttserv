cmake_minimum_required(VERSION 3.16)
project(sttserver_sandbox C CXX)


set(SOURCES
    source/capture_play_mic.cpp
    source/miniaudio_impl.c
    # source/test_sdl3_audio.cpp
)


add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_features(${PROJECT_NAME} INTERFACE 
    c_std_11
    cxx_std_17
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    miniaudio
    whisper
    UTIL2::util2
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