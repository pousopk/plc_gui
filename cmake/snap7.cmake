# src/core/s7_partner.h has a file-scope `using namespace std;` which, under
# C++17 and later, makes the unqualified `byte` in snap7's own headers
# ambiguous with std::byte. Patched out on first fetch (idempotent — safe if
# CMake re-runs the patch step, since re-applying to already-commented text
# is a no-op match failure, not a duplicate comment).
FetchContent_Declare(
  snap7
  GIT_REPOSITORY https://github.com/SCADACS/snap7.git
  GIT_TAG        f6ff90317ca5d54250f4dcd29209689a74e26d82
  SOURCE_DIR     ${CMAKE_SOURCE_DIR}/third_party/snap7
  PATCH_COMMAND  ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/patch_snap7.cmake
)
FetchContent_MakeAvailable(snap7)

add_library(snap7_static STATIC
    ${snap7_SOURCE_DIR}/src/sys/snap_msgsock.cpp
    ${snap7_SOURCE_DIR}/src/sys/snap_sysutils.cpp
    ${snap7_SOURCE_DIR}/src/sys/snap_tcpsrvr.cpp
    ${snap7_SOURCE_DIR}/src/sys/snap_threads.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_client.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_isotcp.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_partner.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_peer.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_server.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_text.cpp
    ${snap7_SOURCE_DIR}/src/core/s7_micro_client.cpp
    ${snap7_SOURCE_DIR}/src/lib/snap7_libmain.cpp
)
target_include_directories(snap7_static PUBLIC
    ${snap7_SOURCE_DIR}/src/sys
    ${snap7_SOURCE_DIR}/src/core
    ${snap7_SOURCE_DIR}/src/lib
    ${snap7_SOURCE_DIR}/examples/plain-c   # public snap7.h (Cli_*/Srv_* declarations)
)
find_package(Threads REQUIRED)
target_link_libraries(snap7_static PUBLIC Threads::Threads)
