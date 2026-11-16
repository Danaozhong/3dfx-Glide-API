# cmake -DOBJCOPY=<objcopy> -DOBJECTS=<object files> -DDEF_FILE=<file>
#       -DLIBRARY=<dll name> -P GlideDefFile.cmake
#
# Writes a .def file exporting everything the objects export with
# __declspec(dllexport). GCC records these exports as "-export:" directives
# in the .drectve section of each object. Decorated __stdcall names
# (name@N) are additionally exported with the MSVC name _name@N.

cmake_minimum_required(VERSION 3.13)

set(exports "")
set(data_exports "")

foreach(object IN LISTS OBJECTS)
    set(drectve ${DEF_FILE}.drectve)
    set(scratch ${DEF_FILE}.objcopy)
    # Fails for objects without a .drectve section, these export nothing.
    execute_process(
        COMMAND ${OBJCOPY} --dump-section .drectve=${drectve} ${object} ${scratch}
        RESULT_VARIABLE result
        OUTPUT_QUIET ERROR_QUIET)
    file(REMOVE ${scratch})
    if(NOT result EQUAL 0 OR NOT EXISTS ${drectve})
        continue()
    endif()

    file(READ ${drectve} directives)
    file(REMOVE ${drectve})

    string(REGEX MATCHALL "-export:(\"[^\"]+\"|[^ ,\"]+)(,data)?" matches "${directives}")
    foreach(match IN LISTS matches)
        string(REGEX MATCH "-export:\"?([^ ,\"]+)\"?(,data)?" match "${match}")
        if(CMAKE_MATCH_2)
            list(APPEND data_exports ${CMAKE_MATCH_1})
        else()
            list(APPEND exports ${CMAKE_MATCH_1})
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES exports)
list(REMOVE_DUPLICATES data_exports)
list(SORT exports)
list(SORT data_exports)

if(NOT exports)
    message(FATAL_ERROR "No exported functions found in the objects for ${LIBRARY}")
endif()

set(content "LIBRARY ${LIBRARY}\nEXPORTS\n")
foreach(name IN LISTS exports)
    string(APPEND content "  ${name}\n")
    if(name MATCHES "^[A-Za-z].*@[0-9]+$")
        string(APPEND content "  _${name} = ${name}\n")
    endif()
endforeach()
foreach(name IN LISTS data_exports)
    string(APPEND content "  ${name} DATA\n")
endforeach()

file(WRITE ${DEF_FILE} "${content}")
