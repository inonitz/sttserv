#ifndef _STTSERVER_API_H_
#define _STTSERVER_API_H_


#ifdef STTSERVER_STATIC_DEFINE /* Define if compiling as a static library (-DSTTSERVER_STATIC_DEFINE) */
#	define STTSERVER_API
#	define STTSERVER_NO_EXPORT
#else
#	ifndef STTSERVER_API
#		ifdef STTSERVER_EXPORTS /* We are building this library */
#			if defined _WIN32 || defined _WIN64 || defined __CYGWIN__ || defined __MINGW64__
#				if defined __GNUC__ || defined __clang__
#					define STTSERVER_API __attribute__ ((dllexport))
#				else
#					define STTSERVER_API __declspec(dllexport)
#				endif
#			else 
#				if (defined __GNUC__ && __GNUC__ >= 4) || defined __clang__
#					define STTSERVER_API __attribute__ ((visibility ("default")))
#				endif
#			endif
#    	else /* We are using this library */
#			if defined _WIN32 || defined _WIN64 || defined __CYGWIN__ || defined __MINGW64__
#				if defined __GNUC__ || defined __clang__
#					define STTSERVER_API __attribute__ ((dllimport))
#				else
#					define STTSERVER_API __declspec(dllimport)
#				endif
#			else 
#				if defined __GNUC__ && __GNUC__ >= 4
#					define STTSERVER_API
#				endif
#			endif
#       endif
#	else /* Should Only reach here for non-*nix, un-supported platforms */
#       warning "Platform Unsupported - Either Not a derivative of Unix // Not Windows"
#		define STTSERVER_API
#   endif
#	ifndef STTSERVER_NO_EXPORT
#       if defined __GNUC__ && __GNUC__ >= 4 /* Symbols exported by default on *nix systems */
#           define STTSERVER_NO_EXPORT __attribute__((visibility ("hidden")))
#       else /* (DLL) Symbols on platforms like windows must be exported manually [__declspec(dllexport)] */
#		    define STTSERVER_NO_EXPORT 
#       endif
#	endif
#endif


#ifndef STTSERVER_DEPRECATED
#   if defined(__cplusplus)
#       if __cplusplus >= 201402L /* [[deprecated]] Supported since C++14 */
#           define STTSERVER_DEPRECATED [[deprecated]]
#           define STTSERVER_DEPRECATED_MSG(MSG) [[deprecated(MSG)]]
#       endif
#   else
#       if defined _WIN32 || defined _WIN64
#           if defined __GNUC__ || defined __clang__ /* Cygwin, MinGW32/64 */
#               define STTSERVER_DEPRECATED          __attribute__((deprecated))
#               define STTSERVER_DEPRECATED_MSG(MSG) __attribute__((deprecated(MSG)))
#           else
#               define STTSERVER_DEPRECATED          __declspec(deprecated)
#               define STTSERVER_DEPRECATED_MSG(MSG) __declspec(deprecated(MSG))
#           endif
#       elif defined __GNUC__ || defined __clang__
#           define STTSERVER_DEPRECATED __attribute__((deprecated))
#           define STTSERVER_DEPRECATED_MSG(MSG) __attribute__((deprecated(MSG)))
#       else /* Should Only reach here for non-*nix, un-supported platforms */
#           define STTSERVER_DEPRECATED
#           define STTSERVER_DEPRECATED_MSG(MSG)
#       endif
#   endif
#endif


#ifndef STTSERVER_DEPRECATED_EXPORT
#  define STTSERVER_DEPRECATED_EXPORT STTSERVER_API STTSERVER_DEPRECATED
#endif


#ifndef STTSERVER_DEPRECATED_NO_EXPORT
#  define STTSERVER_DEPRECATED_NO_EXPORT STTSERVER_NO_EXPORT STTSERVER_DEPRECATED
#endif


/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef STTSERVER_NO_DEPRECATED
#    define STTSERVER_NO_DEPRECATED
#  endif
#endif

#endif /* _STTSERVER_API_H_ */
