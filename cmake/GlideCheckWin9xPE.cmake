# cmake -DOBJDUMP=<objdump> -DBINARY=<exe or dll> -P GlideCheckWin9xPE.cmake
#
# Fails if the PE header or the imported DLLs prevent Windows 9x from loading
# BINARY, or if Glide functions are not exported with the names programs
# built with MSVC import.

cmake_minimum_required(VERSION 3.13)

execute_process(
    COMMAND ${OBJDUMP} -p ${BINARY}
    OUTPUT_VARIABLE headers
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${OBJDUMP} -p ${BINARY} failed")
endif()

set(problems "")

# Windows 9x refuses images that require a newer Windows version than 4.x.
foreach(field MajorOSystemVersion MajorSubsystemVersion)
    if(headers MATCHES "${field}[ \t]+([0-9]+)")
        if(CMAKE_MATCH_1 GREATER 4)
            list(APPEND problems "${field} is ${CMAKE_MATCH_1}, Windows 9x requires 4")
        endif()
    else()
        list(APPEND problems "${field} not found in the output of objdump")
    endif()
endforeach()

string(REGEX MATCHALL "DLL Name: [^\r\n]+" imports "${headers}")
foreach(import IN LISTS imports)
    string(REPLACE "DLL Name: " "" dll "${import}")
    string(STRIP "${dll}" dll)
    string(TOLOWER "${dll}" dll_lower)
    if(dll_lower MATCHES "^(libgcc_s|libwinpthread|libstdc\\+\\+|ucrtbase|api-ms-win-|vcruntime|msvcp|msvcr[0-9])")
        list(APPEND problems "imports ${dll}, which is not available on Windows 9x")
    endif()
endforeach()

# Programs built with MSVC import the __stdcall functions as _name@N.
string(REGEX MATCHALL "\n\t\\[ *[0-9]+\\] [^\r\n]+" export_lines "${headers}")
set(exports "")
foreach(line IN LISTS export_lines)
    string(REGEX REPLACE "^\n\t\\[ *[0-9]+\\] " "" name "${line}")
    string(STRIP "${name}" name)
    list(APPEND exports "${name}")
endforeach()
foreach(name IN LISTS exports)
    if(name MATCHES "^[A-Za-z].*@[0-9]+$" AND NOT "_${name}" IN_LIST exports)
        list(APPEND problems "exports ${name}, but not the MSVC name _${name}")
    endif()
endforeach()

if(problems)
    list(JOIN problems "\n  " details)
    message(FATAL_ERROR
        "${BINARY} is not compatible with Windows 9x Glide applications:\n  ${details}\n"
        "Build with mingw-w64-x86_32.cmake, or disable this check with -DGLIDE_CHECK_WIN9X=OFF.")
endif()
