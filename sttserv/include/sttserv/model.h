#pragma once
#include <sttserv/sttserver_api.h>
#include <sttserv/sttserver_extern.h>
#include <util2/C/base_type.h>
#include <stdbool.h>


STTSERVER_EXTERNC_DECL_BEGIN

typedef struct OpaqueBackendContext*           BackendContextHandle;
typedef struct CommandLineArguments*           CommandLineArgsHandle;
typedef struct OpaqueBackendContextParameters* BackendContextParametersHandle;
typedef u8                                     BackendType;
typedef char                                   BackendTranscriptionResult[1024];

#ifndef BACKEND_TYPE_WHISPERCPP
#   define BACKEND_TYPE_WHISPERCPP ((BackendType)0b00000001)
#endif
#ifndef BACKEND_TYPE_WHISPERCPP_PARAKEET
#   define BACKEND_TYPE_WHISPERCPP_PARAKEET ((BackendType)0b00000010)
#endif
#ifndef BACKEND_TYPE_SHERPAONNX_PARAKEET
#   define BACKEND_TYPE_SHERPAONNX_PARAKEET ((BackendType)0b00000100)
#endif
#ifndef BACKEND_TYPE_SHERPAONNX_WHISPER
#   define BACKEND_TYPE_SHERPAONNX_WHISPER ((BackendType)0b00001000)
#endif
#ifndef BACKEND_TYPE_DEFAULT
#   define BACKEND_TYPE_DEFAULT ((BackendType)0b00000000)
#endif
#ifndef BACKEND_TYPE_MAX
#   define BACKEND_TYPE_MAX ((BackendType)0b11111111)
#endif


STTSERVER_API bool createBackend(
    CommandLineArgsHandle          args, 
    BackendContextParametersHandle inContextSpecificArgsMaybeNull,
    BackendContextHandle*          outContext,
    BackendContextParametersHandle outContextSpecificArgsMaybeNull
);
STTSERVER_API void destroyBackend(BackendContextHandle context);

STTSERVER_API bool transcribe(BackendTranscriptionResult* outResult);


STTSERVER_EXTERNC_DECL_END
