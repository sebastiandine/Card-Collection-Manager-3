# Toolchain detection.
#
# Preferred compiler is Clang. On Windows hosts we additionally fall back to
# MinGW-w64 g++ when Clang is not present. MSVC is intentionally not supported.
#
# Run this BEFORE project() so the resulting CMAKE_C(XX)_COMPILER takes effect.

if(DEFINED CMAKE_CXX_COMPILER)
    # Respect explicit user override.
    return()
endif()

# Look for clang / clang++ first.
find_program(_CCM_CLANG  NAMES clang  clang-cl)
find_program(_CCM_CLANGXX NAMES clang++ clang-cl)

if(_CCM_CLANG AND _CCM_CLANGXX)
    set(CMAKE_C_COMPILER   "${_CCM_CLANG}"   CACHE FILEPATH "C compiler"   FORCE)
    set(CMAKE_CXX_COMPILER "${_CCM_CLANGXX}" CACHE FILEPATH "C++ compiler" FORCE)
    message(STATUS "[ccm] Toolchain: Clang at ${_CCM_CLANGXX}")
    return()
endif()

if(WIN32)
    # MinGW-w64 GCC fallback.
    find_program(_CCM_GCC  NAMES x86_64-w64-mingw32-gcc gcc)
    find_program(_CCM_GXX  NAMES x86_64-w64-mingw32-g++ g++)
    if(_CCM_GCC AND _CCM_GXX)
        set(CMAKE_C_COMPILER   "${_CCM_GCC}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_CCM_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "[ccm] Toolchain: MinGW-w64 GCC at ${_CCM_GXX}")
        return()
    endif()
    message(WARNING
        "[ccm] Neither Clang nor MinGW-w64 GCC was found in PATH. "
        "Install LLVM (preferred) or MSYS2's mingw-w64-x86_64-gcc and re-run."
    )
else()
    # Unix-likes: GCC fallback.
    find_program(_CCM_GCC NAMES gcc cc)
    find_program(_CCM_GXX NAMES g++ c++)
    if(_CCM_GCC AND _CCM_GXX)
        set(CMAKE_C_COMPILER   "${_CCM_GCC}" CACHE FILEPATH "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER "${_CCM_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        message(STATUS "[ccm] Toolchain: GCC at ${_CCM_GXX}")
        return()
    endif()
    message(WARNING "[ccm] Neither Clang nor GCC was found in PATH.")
endif()
