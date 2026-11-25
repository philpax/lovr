#ifndef LOVR_OPENVR_API_H
#define LOVR_OPENVR_API_H

// The SDK typedefs bool when MinGW defines __WIN32. Keep _WIN32 for the Windows ABI.
#ifdef __WIN32
#pragma push_macro("__WIN32")
#undef __WIN32
#include <openvr_capi.h>
#pragma pop_macro("__WIN32")
#else
#include <openvr_capi.h>
#endif

#endif
