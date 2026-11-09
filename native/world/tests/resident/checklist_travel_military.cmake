# 专属依赖门不隐式重建生产库；构建 checklist_K4_1 后由 CTest 启动冷进程，不算整行通过。
foreach(required HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED HANMO_CHECKLIST_OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Travel checklist input is missing: ${required}")
    endif()
endforeach()
foreach(required HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "Travel checklist input does not exist: ${required}")
    endif()
endforeach()
foreach(mode produce consume)
    execute_process(
        COMMAND "${HANMO_CHECKLIST_BINARY}" "${mode}"
            "${HANMO_CHECKLIST_PREPARED}" "${HANMO_CHECKLIST_OUTPUT}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "K4-1 actual ${mode} process failed: ${result}")
    endif()
endforeach()
