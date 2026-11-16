# cmake -DOBJCOPY=<objcopy> -DOBJECT=<fxgasm object file>
#       -DFXGASM_H=<file> -DFXINLINE_H=<file> -P GlideFxGasmExtract.cmake
#
# Extracts the headers written by fxgasm_emit.h (FXGASM_EMIT_SECTIONS mode).

set(end_marker "@@FXGASM_END@@")

function(extract_section section output)
    set(raw ${output}.raw)
    set(scratch ${output}.objcopy)

    execute_process(
        COMMAND ${OBJCOPY} --dump-section ${section}=${raw} ${OBJECT} ${scratch}
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    file(REMOVE ${scratch})
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Extracting section ${section} from ${OBJECT} failed:\n${error}")
    endif()

    file(READ ${raw} content)
    file(REMOVE ${raw})

    string(FIND "${content}" "${end_marker}" end)
    if(end EQUAL -1)
        message(FATAL_ERROR "Section ${section} in ${OBJECT} has no end marker")
    endif()
    string(SUBSTRING "${content}" 0 ${end} content)

    file(WRITE ${output} "${content}")
endfunction()

extract_section(.fxgasm ${FXGASM_H})
extract_section(.fxinl ${FXINLINE_H})
