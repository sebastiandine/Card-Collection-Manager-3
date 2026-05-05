# Shared warning configuration as an INTERFACE library so each first-party
# target can opt in with `target_link_libraries(<tgt> PRIVATE ccm_warnings)`
# without duplicating flags.

add_library(ccm_warnings INTERFACE)

set(_CCM_GCC_CLANG_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    # -Wconversion / -Wsign-conversion are intentionally NOT enabled - they
    # fight badly with wxWidgets's wxWindowID = int API surface and the
    # noise-to-signal ratio doesn't justify it for a single-binary app.
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(ccm_warnings INTERFACE ${_CCM_GCC_CLANG_FLAGS})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ccm_warnings INTERFACE /W4 /permissive-)
endif()

# UTF-8 source/exec encoding everywhere.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ccm_warnings INTERFACE /utf-8)
endif()
