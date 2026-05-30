cmake_minimum_required(VERSION 3.16)
project(whisper_example C CXX)


set(SOURCES
    async_key.cpp
    audio2.cpp
    whisper_init.cpp
    key_codes.cpp
    trunc_data.cpp
)

set(HEADERS
    key_codes.hpp
    async_key.hpp
    audio2.hpp
    whisper_init.hpp
    trunc_data.hpp
)


add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${HEADERS})
target_compile_features(${PROJECT_NAME} PRIVATE 
    c_std_11
    cxx_std_17
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    miniaudio
    whisper
    UTIL2::util2
    cxxopts::cxxopts
    # STTSERVER::SpeechToTextServer
    WORKSPACE_CONFIG::ProjectDefaultConfig
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