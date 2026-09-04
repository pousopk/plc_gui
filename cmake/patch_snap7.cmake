# Patches applied to the vendored snap7 source on every configure.
# FetchContent re-checks PATCH_COMMAND beyond just the first populate, so
# each patch must be genuinely idempotent: guard on whether its marker
# comment is already present rather than assuming string(REPLACE) is safe
# to reapply (a plain replace can match its own output on a second run).

function(patch_once target_file marker old_text new_text)
    file(READ "${target_file}" file_contents)
    string(FIND "${file_contents}" "${marker}" already_patched)
    if(already_patched EQUAL -1)
        string(REPLACE "${old_text}" "${new_text}" file_contents "${file_contents}")
        file(WRITE "${target_file}" "${file_contents}")
    endif()
endfunction()

set(snap7_src "${CMAKE_CURRENT_LIST_DIR}/../third_party/snap7/src")

# src/core/s7_partner.h has a file-scope `using namespace std;` which, under
# C++17 and later, makes the unqualified `byte` used throughout snap7's own
# headers ambiguous with std::byte.
patch_once(
    "${snap7_src}/core/s7_partner.h"
    "removed: collides with std::byte"
    "using namespace std;"
    "// using namespace std; -- removed: collides with std::byte under C++17+"
)

# PS7AreaContainer's constructor (src/core/s7_server.cpp) allocates
# `new PS7Area[size]` — an array of *pointers* — but zeroes only `size`
# bytes instead of `size * sizeof(PS7Area)`. PS7Area is an 8-byte pointer
# on 64-bit, and containers are sized MaxDB-1 = 2047, so only the first
# ~256 of 2047 slots ever get zeroed; the rest are uninitialized garbage
# that Dispose() later misreads as live pointers and dereferences,
# segfaulting on server teardown. Confirmed via AddressSanitizer against a
# real Srv_Create/Srv_RegisterArea/Srv_Destroy cycle — a genuine
# memory-safety bug in the vendored library, not a usage error.
patch_once(
    "${snap7_src}/core/s7_server.cpp"
    "memset(area, 0, size * sizeof(PS7Area))"
    "memset(area, 0, size);"
    "memset(area, 0, size * sizeof(PS7Area)); // patched: was `size` (bytes), undersizing a pointer array"
)

# PS7AreaContainer::Dispose() (same file) frees `area` — allocated with
# `new PS7Area[size]` — via plain `delete area;` instead of `delete[]`, an
# alloc/dealloc mismatch (undefined behavior). Also confirmed via
# AddressSanitizer, surfacing only once the memset bug above no longer
# masked it with an earlier crash.
patch_once(
    "${snap7_src}/core/s7_server.cpp"
    "patched: was `delete area`"
    "    delete area;
}"
    "    delete[] area; // patched: was `delete area` on a `new PS7Area[size]` array (alloc-dealloc mismatch)
}"
)
