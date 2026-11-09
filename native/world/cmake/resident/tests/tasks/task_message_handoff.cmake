# 任务与履约消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 实物文书交接保守权源与冷记录门；不替代整World/GUI。
add_executable(test_resident_task_message_handoff tests/resident/task_message_handoff_test.cpp)
target_link_libraries(test_resident_task_message_handoff PRIVATE hanmo_resident)
target_compile_options(test_resident_task_message_handoff PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
