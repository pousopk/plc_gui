# src/core/s7_partner.h has a file-scope `using namespace std;` which, under
# C++17 and later, makes the unqualified `byte` used throughout snap7's own
# headers ambiguous with std::byte. Strip it. Idempotent: if already patched,
# string(REPLACE) simply finds no match and rewrites the file unchanged.
set(target_file "${CMAKE_CURRENT_LIST_DIR}/../third_party/snap7/src/core/s7_partner.h")
file(READ "${target_file}" file_contents)
string(REPLACE
    "using namespace std;"
    "// using namespace std; -- removed: collides with std::byte under C++17+"
    file_contents "${file_contents}")
file(WRITE "${target_file}" "${file_contents}")
