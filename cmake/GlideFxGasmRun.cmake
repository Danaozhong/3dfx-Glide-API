# cmake -DTOOL=<fxgasm executable> -DARGUMENT=<-hex|-inline> -DOUTPUT=<file> -P GlideFxGasmRun.cmake

execute_process(
    COMMAND ${TOOL} ${ARGUMENT}
    OUTPUT_FILE ${OUTPUT}
    RESULT_VARIABLE result)

if(NOT result EQUAL 0)
    file(REMOVE ${OUTPUT})
    message(FATAL_ERROR "${TOOL} ${ARGUMENT} failed: ${result}")
endif()
