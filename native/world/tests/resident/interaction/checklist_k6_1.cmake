# 只执行本行已构建二进制；生产和冷读必须是两个独立进程。
foreach(required HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED HANMO_CHECKLIST_OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Required test input is missing: ${required}")
    endif()
endforeach()
# 保留每轮原待消息及续接银行，不用新跑结果覆盖此前证据。
string(TIMESTAMP run_id "%Y%m%dT%H%M%SZ" UTC)
set(run_output "${HANMO_CHECKLIST_OUTPUT}/${run_id}")
# 同轮两进程只读本轮实际输入副本，避免别域合法追加数据时冷热身份竞争。
file(MAKE_DIRECTORY "${run_output}")
set(run_prepared "${run_output}/prepared.json")
file(COPY_FILE "${HANMO_CHECKLIST_PREPARED}" "${run_prepared}")
message(STATUS "Actual K6-1 evidence: ${run_output}")
foreach(mode produce consume)
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" "${mode}"
        "${run_prepared}" "${run_output}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "K6-1 ${mode} failed: ${result}")
    endif()
endforeach()
