cmake_minimum_required(VERSION 3.20)
project(dbg LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "This project is MSVC-only. Please configure with MSVC (cl.exe).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force UTF-8 source/execution character set for MSVC.
add_compile_options(/utf-8)

# Keep one CRT choice across all targets/subprojects to avoid LNK4098.
# /MD for Release, /MDd for Debug.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(ZYDIS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cloned_repos/zydis")
if(NOT EXISTS "${ZYDIS_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing Zydis source tree at ${ZYDIS_SOURCE_DIR}")
endif()

set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_MAN OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(ZYAN_SYSTEM_ZYCORE OFF CACHE BOOL "" FORCE)
add_subdirectory("${ZYDIS_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/zydis" EXCLUDE_FROM_ALL)

add_library(dbg_backend SHARED
    src/backend/src/api/dbg_api.cpp
    src/backend/src/core/debugger.cpp
    src/backend/src/core/debug_loop.cpp
    src/backend/src/core/process_control.cpp
    src/backend/src/core/threads.cpp
    src/backend/src/core/modules.cpp
    src/backend/src/core/memory.cpp
    src/backend/src/core/memory_region.cpp
    src/backend/src/core/registers.cpp
    src/backend/src/core/run_control.cpp
    src/backend/src/core/breakpoint.cpp
    src/backend/src/analysis/disassembler.cpp
    src/backend/src/analysis/instruction_formatter.cpp
)

target_compile_definitions(dbg_backend PRIVATE
    DBG_BACKEND_BUILD
    ZYDIS_STATIC_BUILD
    ZYCORE_STATIC_BUILD
)

target_include_directories(dbg_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/src
)

target_link_libraries(dbg_backend PRIVATE
    Zydis::Zydis
)

set(DBG_FRONTEND_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/dbg/lib")

set_target_properties(dbg_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DBG_FRONTEND_LIB_DIR}"
)
