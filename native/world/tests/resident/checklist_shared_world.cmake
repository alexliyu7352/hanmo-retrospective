# 先构建实际测试消费者，再等待保存进程退出后启动独立冷恢复进程；任何阶段失败都中止。
foreach(required HANMO_BUILD_DIR HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED HANMO_CHECKLIST_OUTPUT HANMO_CHECKLIST_DAYS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Required test input is missing: ${required}")
    endif()
endforeach()
if(NOT EXISTS "${HANMO_CHECKLIST_PREPARED}")
    message(FATAL_ERROR "Generate the selected shared_world_foundation fixture first: ${HANMO_CHECKLIST_PREPARED}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${HANMO_BUILD_DIR}"
    --target test_resident_checklist_shared_world --parallel 1
    RESULT_VARIABLE built)
if(NOT built EQUAL 0)
    message(FATAL_ERROR "Shared world test consumer build failed: ${built}")
endif()
foreach(mode produce consume)
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" "${mode}"
        "${HANMO_CHECKLIST_PREPARED}" "${HANMO_CHECKLIST_OUTPUT}" "${HANMO_CHECKLIST_DAYS}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Shared world ${mode} failed: ${result}")
    endif()
endforeach()
