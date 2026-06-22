#ifndef __LLM_ASR_MODEL_UNIFIED_INTERFACE_LLM_BACKEND_USED_DEFINITION_HEADER__
#define __LLM_ASR_MODEL_UNIFIED_INTERFACE_LLM_BACKEND_USED_DEFINITION_HEADER__
#   include <sttserv/sttserver_extern.h>
#   include <util2/C/compiler_warning.h>

#   if defined(__cplusplus)
        enum class BackendType { 
            WHISPER     = 0, 
            PARAKEET    = 1, 
            SHERPA_ONNX = 2, 
            BACKEND_MAX = 3 
        };
#   else
#       ifdef STTSERV_BACKEND_TYPE_WHISPER
#          pragma WARN("There is someone here impersonating STTSERV_BACKEND_TYPE_WHISPER... You should find out why ")
#       endif
#       ifdef STTSERV_BACKEND_TYPE_PARAKEET
#          pragma WARN("There is someone here impersonating STTSERV_BACKEND_TYPE_PARAKEET... You should find out why ")
#       endif
#       ifdef STTSERV_BACKEND_TYPE_SHERPA_ONNX
#          pragma WARN("There is someone here impersonating STTSERV_BACKEND_TYPE_SHERPA_ONNX... You should find out why ")
#       endif
#       ifdef STTSERV_BACKEND_TYPE_BACKEND_MAX
#          pragma WARN("There is someone here impersonating STTSERV_BACKEND_TYPE_BACKEND_MAX... You should find out why ")
#       endif
#       ifndef STTSERV_BACKEND_TYPE_WHISPERCPP
#          define BACKEND_TYPE_WHISPERCPP ((BackendType)0b00000001)
#       endif
#       ifndef STTSERV_BACKEND_TYPE_WHISPERCPP_PARAKEET
#          define BACKEND_TYPE_WHISPERCPP_PARAKEET ((BackendType)0b00000010)
#       endif
#       ifndef STTSERV_BACKEND_TYPE_SHERPAONNX_PARAKEET
#          define BACKEND_TYPE_SHERPAONNX_PARAKEET ((BackendType)0b00000100)
#       endif
#       ifndef STTSERV_BACKEND_TYPE_SHERPAONNX_WHISPER
#          define BACKEND_TYPE_SHERPAONNX_WHISPER ((BackendType)0b00001000)
#       endif
#       ifndef STTSERV_BACKEND_TYPE_DEFAULT
#          define BACKEND_TYPE_DEFAULT ((BackendType)0b00000000)
#       endif
#       ifndef STTSERV_BACKEND_TYPE_MAX
#          define BACKEND_TYPE_MAX ((BackendType)0b11111111)
#       endif
        typedef u8 BackendType;
#   endif


#endif /* __LLM_ASR_MODEL_UNIFIED_INTERFACE_LLM_BACKEND_USED_DEFINITION_HEADER__ */
