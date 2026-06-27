cmake_minimum_required(VERSION 3.16)
project(sttserver 
    VERSION 0.0.1
    DESCRIPTION "C/C++ Cross-Platform Multi-Runtime Nonsense"
    LANGUAGES C CXX
)


set(SOURCES
    source/async_key.cpp
    source/audio2.cpp
    source/backend.cpp
    source/cmdline.cpp
    source/key_codes.cpp
)

set(HEADERS
    include/sttserv/async_key.hpp
    include/sttserv/audio2.hpp
    include/sttserv/backend.hpp
    include/sttserv/backendType.hpp
    include/sttserv/cmdline.hpp
    include/sttserv/key_codes.hpp
    include/sttserv/sttserver_api.h
    include/sttserv/sttserver_extern.h
    include/sttserv/wav_writer.hpp
)




# Backend to choose 
set(RUNTIME_BACKEND_LIBRARY_NAME RuntimeModelBackend)
add_library(${RUNTIME_BACKEND_LIBRARY_NAME} INTERFACE)
# target_compile_definitions(${RUNTIME_BACKEND_LIBRARY_NAME} INTERFACE
#     $<$<BOOL:${STTSERVER_BUILD_BACKEND_WHISPER}>:STTSERVER_BUILD_BACKEND_WHISPER>
#     $<$<BOOL:${STTSERVER_BUILD_BACKEND_PARAKEET}>:STTSERVER_BUILD_BACKEND_PARAKEET>
#     $<$<BOOL:${STTSERVER_BUILD_BACKEND_SHERPA_ONNX}>:STTSERVER_BUILD_BACKEND_SHERPA_ONNX>
# )
target_link_libraries(${RUNTIME_BACKEND_LIBRARY_NAME} INTERFACE
    $<$<BOOL:${STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER}>:whisper>
    $<$<BOOL:${STTSERVER_BUILD_LIBRARY_BACKEND_PARAKEET}>:parakeet>
    $<$<BOOL:${STTSERVER_BUILD_LIBRARY_BACKEND_PARAKEET}>:whisper>
    $<$<BOOL:${STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX}>:sherpa-onnx-c-api>
    $<$<BOOL:${STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX}>:sherpa-onnx-cxx-api>
)


# The actual Utility Library
if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED)
    target_compile_definitions(${PROJECT_NAME} PUBLIC STTSERVER_EXPORTS)
else()
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC STTSERVER_STATIC_DEFINE)
endif()

add_library(STTSERVER::SpeechToTextServer ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${HEADERS})

target_compile_features(${PROJECT_NAME} PRIVATE 
    c_std_11
    cxx_std_17
)
target_link_libraries(${PROJECT_NAME} PUBLIC 
    STTSERVER_WORKSPACE_CONFIG::ProjectDefaultConfig
    ${RUNTIME_BACKEND_LIBRARY_NAME}
    miniaudio
    UTIL2::util2
    cxxopts::cxxopts
)


# Executable Server Target