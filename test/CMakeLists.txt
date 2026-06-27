cmake_minimum_required(VERSION 3.17)
include(GoogleTest)
project(accuracy_test C CXX)


enable_testing()


set(SOURCES
    asr_test.cpp
    main.cpp
)

set(HEADERS
    asr_test.hpp
)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${HEADERS})
target_compile_features(${PROJECT_NAME} PRIVATE 
    c_std_11
    cxx_std_17
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    cxxopts::cxxopts
    GTest::gtest
    STTSERVER::SpeechToTextServer
    STTSERVER_WORKSPACE_CONFIG::ProjectDefaultConfig
)


# function(add_asr_accuracy_test MODEL_PREFIX)
#     gtest_discover_tests(${PROJECT_NAME} TEST_LIST_DISCOVERED_GTESTS
#         PROPERTIES 
#             LABELS      "ASR_Accuracy"
#             ENVIRONMENT "GTEST_COLOR=yes"
#         TEST_PREFIX "${MODEL_PREFIX}."
#         EXTRA_ARGS  ${ARGN} 
#     )
# endfunction()

# function(add_asr_accuracy_test MODEL_PREFIX)
#     add_test(NAME "ASR_Accuracy.${MODEL_PREFIX}"
#         COMMAND ${PROJECT_NAME} ${ARGN} # ARGN captures all remaining arguments automatically
#     )
    
#     set_tests_properties("ASR_Accuracy.${MODEL_PREFIX}" 
#         PROPERTIES 
#             LABELS      "ASR_Accuracy"
#             ENVIRONMENT "GTEST_COLOR=yes"
#     )
# endfunction()


function(add_asr_accuracy_test MODEL_PREFIX)
    add_custom_target(run_${MODEL_PREFIX}
        COMMAND ${PROJECT_NAME} ${ARGN}
        USES_TERMINAL
        COMMENT "Running direct accuracy test: ${MODEL_PREFIX}"
    )
endfunction()


add_asr_accuracy_test(parakeet-v3-f32-whisper-backend
    --model=../../../../dependencies/models/nvidia--parakeet-tdt-0.6b-v3/ggml-parakeet-tdt-0.6b-v3-f32.bin
    --backend=whisper-parakeet
    --fa
    --gid=1
    --threads=2
    --captureid=1
    --playbackid=0
)

add_asr_accuracy_test(distil-whisper-large-v3.5
    --model=../../../../dependencies/models/distil-whisper-large-v3.5/ggml-model-q5k.bin
    --backend=whisper-whisper
    --fa
    --threads=1
    --gid=1
    --captureid=1
    --playbackid=0
)

add_asr_accuracy_test(sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-fp16
    --fa
    --threads=2
    --captureid=1
    --playbackid=0
    --backend=sherpaonnx-parakeet
    --mfp=../../../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-fp16
    --en=encoder.fp16.onnx
    --de=decoder.fp16.onnx
    --jo=joiner.fp16.onnx
    --tok=tokens.txt
)

add_asr_accuracy_test(sherpa-onnx-nemo-parakeet-unified-en-0.6b-int8-non-streaming
    --fa
    --threads=2
    --captureid=1
    --playbackid=0
    --backend=sherpaonnx-parakeet
    --mfp=../../../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-unified-en-0.6b-int8-non-streaming
    --en=encoder.int8.onnx
    --de=decoder.int8.onnx
    --jo=joiner.int8.onnx
    --tok=tokens.txt
)

add_asr_accuracy_test(sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8
    --fa
    --threads=2
    --captureid=1
    --playbackid=0
    --backend=sherpaonnx-parakeet
    --mfp=../../../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8
    --en=encoder.int8.onnx
    --de=decoder.int8.onnx
    --jo=joiner.int8.onnx
    --tok=tokens.txt
)




add_custom_target(${PROJECT_NAME}_run_all
    COMMAND ${CMAKE_CTEST_COMMAND}
        --verbose
        # -j${NPROC}
        --progress
        --output-on-failure
        --test-output-size-passed 262144
    USES_TERMINAL
    WORKING_DIRECTORY ${WORKSPACE_GLOBAL_CUSTOM_BUILD_OUTPUT_DIRECTORY}/../test
    COMMENT "Running unit tests in parallel..."
)


add_custom_target(${PROJECT_NAME}_debug
    COMMAND ./${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${WORKSPACE_GLOBAL_CUSTOM_BUILD_OUTPUT_DIRECTORY}
    USES_TERMINAL
    COMMENT "Running googletest main..."
)
