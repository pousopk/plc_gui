# src/core/s7_partner.h has a file-scope `using namespace std;` which, under
# C++17 and later, makes the unqualified `byte` used throughout snap7's own
# headers ambiguous with std::byte. Strip it.
#
# This runs on every configure (FetchContent re-checks PATCH_COMMAND beyond
# just the first populate), so it must be genuinely idempotent: a plain
# string(REPLACE) of "using namespace std;" would match its own substring
# inside the replacement comment on the second run and re-wrap it forever.
# Guard on whether the marker is already present instead.
set(target_file "${CMAKE_CURRENT_LIST_DIR}/../third_party/snap7/src/core/s7_partner.h")
file(READ "${target_file}" file_contents)
string(FIND "${file_contents}" "removed: collides with std::byte" already_patched)
if(already_patched EQUAL -1)
    string(REPLACE
        "using namespace std;"
        "// using namespace std; -- removed: collides with std::byte under C++17+"
        file_contents "${file_contents}")
    file(WRITE "${target_file}" "${file_contents}")
endif()
