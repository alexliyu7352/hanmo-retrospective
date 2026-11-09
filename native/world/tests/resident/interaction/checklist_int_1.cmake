# 独立命名门只运行已经构建的本行消费者；不触发共享全树构建。
foreach(required HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED HANMO_CHECKLIST_OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Required test input is missing: ${required}")
    endif()
endforeach()
if(NOT EXISTS "${HANMO_CHECKLIST_PREPARED}")
    message(FATAL_ERROR "Selected shared_world_foundation input is missing")
endif()
# 每轮保留原银行，重跑不覆盖首红或此前通过的实际结果。
string(TIMESTAMP run_id "%Y%m%dT%H%M%SZ" UTC)
set(run_output "${HANMO_CHECKLIST_OUTPUT}/${run_id}")
# 同轮两进程只读本轮实际输入副本，避免别域合法追加数据时冷热身份竞争。
file(MAKE_DIRECTORY "${run_output}")
set(run_prepared "${run_output}/prepared.json")
file(COPY_FILE "${HANMO_CHECKLIST_PREPARED}" "${run_prepared}")
message(STATUS "Actual INT-1 evidence: ${run_output}")
foreach(mode produce consume)
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" "${mode}"
        "${run_prepared}" "${run_output}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "INT-1 ${mode} failed: ${result}")
    endif()
endforeach()
