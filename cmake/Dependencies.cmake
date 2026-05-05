# Third-party dependencies pulled via FetchContent. Pinning to known-good tags
# is intentional - bump deliberately, never use `master`.

include(FetchContent)

# Avoid hammering the network on every reconfigure once content is downloaded.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# CMake 4 dropped compatibility with cmake_minimum_required(VERSION < 3.5).
# Some pinned tags below (doctest 2.4.11, nlohmann/json 3.11.3, wxWidgets 3.2.5)
# still ship a `cmake_minimum_required(VERSION 3.0/3.1)` and would otherwise
# fail to configure. Override the floor for all FetchContent'd subprojects.
# See: https://cmake.org/cmake/help/latest/variable/CMAKE_POLICY_VERSION_MINIMUM.html
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# ---------------------------------------------------------------------------
# nlohmann/json - JSON (de)serialization. Header-only.
# ---------------------------------------------------------------------------
message(STATUS "[ccm] Fetching nlohmann/json ...")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# cpr - C++ Requests (libcurl wrapper). Builds curl in-tree so we don't need
# a system libcurl. Used for Scryfall + pokemontcg.io REST calls.
#
# Pinned at 1.10.5 deliberately. 1.11.x adds an `install(EXPORT cprTargets)`
# rule that references `libcurl_shared`, which isn't in any export set when
# curl is built as a sub-project here - configure fails with:
#   "install(EXPORT \"cprTargets\" ...) includes target \"cpr\" which requires
#    target \"libcurl_shared\" that is not in any export set."
# Bumping past 1.10.5 needs additional plumbing (either disable cpr's install
# rules or move curl to its own export set). Verified end-to-end on MSYS2
# UCRT64 GCC 15.2 + CMake 4.3 with this pin.
# ---------------------------------------------------------------------------
message(STATUS "[ccm] Fetching libcpr/cpr (this also fetches a curl source tree) ...")
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.10.5
    GIT_SHALLOW    TRUE
)
set(CPR_USE_SYSTEM_CURL OFF CACHE INTERNAL "")
set(CPR_BUILD_TESTS OFF CACHE INTERNAL "")
set(CPR_ENABLE_SSL ON CACHE INTERNAL "")
# Let cpr fetch & build a matching curl, with a Windows-friendly SSL backend.
set(CPR_FORCE_USE_SYSTEM_CURL OFF CACHE INTERNAL "")

# Trim the curl build aggressively. We only need HTTPS GET, so anything else
# is dead weight at best and a MinGW-w64 link-failure source at worst.
set(BUILD_CURL_EXE         OFF CACHE INTERNAL "")
set(BUILD_TESTING          OFF CACHE INTERNAL "")
set(CURL_DISABLE_TESTS     ON  CACHE INTERNAL "")
set(CURL_DISABLE_DOCS      ON  CACHE INTERNAL "")
set(CURL_DISABLE_INSTALL   ON  CACHE INTERNAL "")
set(CURL_DISABLE_LDAP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_LDAPS     ON  CACHE INTERNAL "")
set(CURL_DISABLE_FTP       ON  CACHE INTERNAL "")
set(CURL_DISABLE_TELNET    ON  CACHE INTERNAL "")
set(CURL_DISABLE_DICT      ON  CACHE INTERNAL "")
set(CURL_DISABLE_FILE      ON  CACHE INTERNAL "")
set(CURL_DISABLE_TFTP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_GOPHER    ON  CACHE INTERNAL "")
set(CURL_DISABLE_IMAP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_POP3      ON  CACHE INTERNAL "")
set(CURL_DISABLE_SMB       ON  CACHE INTERNAL "")
set(CURL_DISABLE_SMTP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_RTSP      ON  CACHE INTERNAL "")
set(CURL_DISABLE_MQTT      ON  CACHE INTERNAL "")

if(WIN32)
    # Use the OS-provided TLS stack so we don't need OpenSSL on the build host.
    set(CURL_USE_SCHANNEL  ON  CACHE INTERNAL "")
    set(CMAKE_USE_SCHANNEL ON  CACHE INTERNAL "")
    set(CURL_USE_OPENSSL   OFF CACHE INTERNAL "")

    # Workaround: curl's CMake compile-test for ioctlsocket(FIONBIO) sometimes
    # fails on MinGW-w64 UCRT64 due to winsock2.h header ordering, which then
    # makes nonblock.c #error out with "no non-blocking method was found".
    # Force-set the macro so the build proceeds; ioctlsocket+FIONBIO is the
    # canonical Win32 API for non-blocking sockets and is always available.
    set(HAVE_IOCTLSOCKET_FIONBIO ON CACHE INTERNAL "")
endif()
FetchContent_MakeAvailable(cpr)

# ---------------------------------------------------------------------------
# wxWidgets - cross-platform UI toolkit.
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# doctest - lightweight, header-only unit testing framework. Only fetched when
# tests are enabled, so a default release build doesn't pull it.
# ---------------------------------------------------------------------------
if(CCM_BUILD_TESTS)
    message(STATUS "[ccm] Fetching doctest ...")
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
    )
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(doctest)
endif()

if(CCM_USE_SYSTEM_WX)
    message(STATUS "[ccm] Using system wxWidgets via find_package")
    find_package(wxWidgets REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    # Re-export as a cleaner imported target so the rest of the build can
    # depend on `wx::wx` regardless of how it was provided.
    add_library(wx::wx INTERFACE IMPORTED)
    target_include_directories(wx::wx INTERFACE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(wx::wx INTERFACE ${wxWidgets_DEFINITIONS})
    target_link_libraries(wx::wx INTERFACE ${wxWidgets_LIBRARIES})
else()
    message(STATUS "[ccm] Fetching wxWidgets (slow on first configure) ...")
    set(wxBUILD_SHARED OFF CACHE INTERNAL "")
    set(wxBUILD_PRECOMP OFF CACHE INTERNAL "")
    set(wxBUILD_INSTALL OFF CACHE INTERNAL "")
    set(wxUSE_STL ON CACHE INTERNAL "")
    set(wxUSE_GUI ON CACHE INTERNAL "")
    FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG        v3.2.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(wxWidgets)
    # wxWidgets provides `wx::core`, `wx::base` etc. as targets when used as
    # a sub-project. Aggregate the ones we need into `wx::wx`.
    if(NOT TARGET wx::wx)
        add_library(wx::wx INTERFACE IMPORTED)
        target_link_libraries(wx::wx INTERFACE wx::core wx::base)
    endif()
endif()
