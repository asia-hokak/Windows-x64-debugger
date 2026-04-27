#pragma once

#if defined(_WIN32)
  #if defined(DBG_BACKEND_BUILD)
    #define DBG_EXPORT __declspec(dllexport)
  #else
    #define DBG_EXPORT __declspec(dllimport)
  #endif
#else
  #define DBG_EXPORT
#endif
