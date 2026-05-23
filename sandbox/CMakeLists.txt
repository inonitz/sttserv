cmake_minimum_required(VERSION 3.16)
project(sttserver_sandbox C CXX)


set(SOURCES
    source/async_key.cpp
    source/audio2.cpp
    source/miniaudio_impl.c
    source/key_codes.cpp
    # source/whisper_init.cpp
)

set(HEADERS

    include/sandbox/key_codes.hpp
    include/sandbox/async_key.hpp
    include/sandbox/audio2.hpp
    # include/sandbox/whisper_init.hpp
)


add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${HEADERS})
target_sources(${PROJECT_NAME} PRIVATE
    # source/examples/11poc_optimize_e2e.cpp
    # source/examples/10poc_trunc_data.cpp
    # include/sandbox/examples/poc_trunc_data.hpp
    # source/examples/10poc_trunc_data/trunc_data.cpp
    source/examples/13poc_onnxruntime/onnx_test2.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(${PROJECT_NAME} PRIVATE 
    c_std_11
    cxx_std_17
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    sherpa-onnx-cxx-api
    miniaudio
    # whisper
    UTIL2::util2
    cxxopts::cxxopts
    # WORKSPACE_CONFIG::ProjectDefaultConfig
)




if(ENABLE_PROFILING)
    message(STATUS "Building Profiled Application!")
    set(PROJECT_EXE_PROFILE_NAME 
        ${PROJECT_NAME}_profile
    )
    add_executable(${PROJECT_EXE_PROFILE_NAME})
    target_sources(${PROJECT_EXE_PROFILE_NAME} PRIVATE ${SOURCES})
    target_sources(${PROJECT_EXE_PROFILE_NAME} PRIVATE ${HEADERS})
    target_sources(${PROJECT_EXE_PROFILE_NAME} PRIVATE
        # source/examples/10poc_trunc_data.cpp
        # include/sandbox/examples/poc_trunc_data.hpp
        
    )
    target_include_directories(${PROJECT_EXE_PROFILE_NAME} PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_compile_features(${PROJECT_EXE_PROFILE_NAME} PRIVATE 
        c_std_11
        cxx_std_17
    )
    target_compile_definitions(${PROJECT_EXE_PROFILE_NAME} PRIVATE
        ENABLE_PROFILING=1
        TRACY_ENABLE=1
        TRACY_CALLSTACK=20
    )
    target_compile_definitions(TracyClient PUBLIC
        TRACY_ENABLE=1
        TRACY_CALLSTACK=20
    )
    target_link_libraries(${PROJECT_EXE_PROFILE_NAME} PRIVATE 
        miniaudio
        crispasr
        UTIL2::util2
        cxxopts::cxxopts
        Tracy::TracyClient
        # WORKSPACE_CONFIG::ProjectDefaultConfig
    )
endif()



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