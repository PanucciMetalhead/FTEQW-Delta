# ISO C17 is the language baseline; OS APIs and documented compiler attributes
# remain available. Extra diagnostics are an audit mode, not a warning waiver.
option(FTE_WERROR "Treat all compiler warnings as errors." OFF)
option(FTE_EXTRA_WARNINGS "Audit legacy code with additional conversion and extension warnings." OFF)
option(FTE_SANITIZERS "Instrument debug builds with AddressSanitizer and UndefinedBehaviorSanitizer." OFF)

if(MSVC AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(fte_c_warnings /W3 /we4013 /we4020 /we4715)
    if(FTE_WERROR)
        list(APPEND fte_c_warnings /WX)
    endif()
    if(FTE_EXTRA_WARNINGS)
        list(APPEND fte_c_warnings /W4)
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(fte_c_warnings
        -Wall -Wformat -Wformat-security -Wstrict-prototypes -Wold-style-definition
        -Werror=implicit-function-declaration -Werror=implicit-int
        -Werror=incompatible-pointer-types -Werror=return-type
        -Werror=format-security -Werror=strict-prototypes -Werror=old-style-definition
        # Byte strings use both signed and unsigned char throughout the legacy API.
        -Wno-pointer-sign -Wno-unknown-pragmas)
    if(FTE_WERROR)
        list(APPEND fte_c_warnings -Werror)
    endif()
    if(FTE_EXTRA_WARNINGS)
        list(APPEND fte_c_warnings -Wextra -Wformat=2 -Wconversion -Wpedantic -Wcast-align)
    endif()
endif()

# Do not apply C-only diagnostics to the C++ IQM tool or physics dependencies.
foreach(fte_warning IN LISTS fte_c_warnings)
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:${fte_warning}>")
endforeach()
unset(fte_c_warnings)
unset(fte_warning)

if(FTE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" OR WIN32)
        message(FATAL_ERROR "FTE_SANITIZERS requires GCC or Clang on Linux/macOS.")
    endif()
    if(CMAKE_CONFIGURATION_TYPES OR NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "FTE_SANITIZERS requires a single-configuration Debug build.")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined)
    # Darwin omits glibc's nonnull contracts for zero-length libc operations.
    # Make native C sanitizer runs check the same preconditions on both hosts.
    if(APPLE)
        add_compile_options("$<$<COMPILE_LANGUAGE:C>:-include>"
            "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_LIST_DIR}/FTESanitizerContracts.h>")
    endif()
endif()
