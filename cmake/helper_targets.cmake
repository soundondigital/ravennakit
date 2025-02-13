add_library(rav_recommended_warning_flags INTERFACE)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") OR (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    target_compile_options(rav_recommended_warning_flags INTERFACE "/W4" "/wd4702" "/we4668")
elseif ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
    target_compile_options(rav_recommended_warning_flags INTERFACE
            -Wall
            -Wundef
            -Wshadow-all
            -Wshorten-64-to-32
            -Wstrict-aliasing
            -Wuninitialized
            -Wunused-parameter
            -Wconversion
            -Wsign-compare
            -Wint-conversion
            -Wconditional-uninitialized
            -Wconstant-conversion
            -Wsign-conversion
            -Wbool-conversion
            -Wextra-semi
            -Wunreachable-code
            -Wcast-align
            -Wshift-sign-overflow
            -Wmissing-prototypes
            -Wnullable-to-nonnull-conversion
            -Wignored-qualifiers
            -Wswitch-enum
            -Wpedantic
            -Wdeprecated
            -Wfloat-equal
            -Wmissing-field-initializers
            $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:OBJCXX>>:
            -Wzero-as-null-pointer-constant
            -Wunused-private-field
            -Woverloaded-virtual
            -Wreorder
            -Winconsistent-missing-destructor-override>
            $<$<OR:$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:
            -Wunguarded-availability
            -Wunguarded-availability-new>)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rav_recommended_warning_flags INTERFACE
            -Wall
            -Wundef
            -Wextra
            -Wpedantic
            -Wstrict-aliasing
            -Wuninitialized
            -Wunused-parameter
            -Wsign-compare
            -Wsign-conversion
            -Wunreachable-code
            -Wcast-align
            -Wimplicit-fallthrough
            -Wmaybe-uninitialized
            -Wignored-qualifiers
            -Wswitch-enum
            -Wredundant-decls
            -Wstrict-overflow
            -Wshadow
            -Wfloat-equal
            -Wmissing-field-initializers
            $<$<COMPILE_LANGUAGE:CXX>:
            -Woverloaded-virtual
            -Wreorder
            -Wzero-as-null-pointer-constant>)
endif ()

add_library(rav_recommended_lto_flags INTERFACE)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") OR (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    target_compile_options(rav_recommended_lto_flags INTERFACE
            $<$<CONFIG:Release>:$<IF:$<STREQUAL:"${CMAKE_CXX_COMPILER_ID}","MSVC">,-GL,-flto>>)
    target_link_libraries(rav_recommended_lto_flags INTERFACE
            $<$<CONFIG:Release>:$<$<STREQUAL:"${CMAKE_CXX_COMPILER_ID}","MSVC">:-LTCG>>)
elseif ((NOT MINGW) AND ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")))
    target_compile_options(rav_recommended_lto_flags INTERFACE $<$<CONFIG:Release>:-flto>)
    target_link_libraries(rav_recommended_lto_flags INTERFACE $<$<CONFIG:Release>:-flto>)
    # Xcode 15.0 requires this flag to avoid a compiler bug
    target_link_libraries(rav_recommended_lto_flags INTERFACE
            $<$<CONFIG:Release>:$<$<STREQUAL:"${CMAKE_CXX_COMPILER_ID}","AppleClang">:-Wl,-weak_reference_mismatches,weak>>)
endif ()

