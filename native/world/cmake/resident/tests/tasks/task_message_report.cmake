# 任务与履约消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 真实送信后当面返报的原结酬与知情来源风险门。
add_executable(test_resident_task_message_report tests/resident/task_message_report_test.cpp)
target_link_libraries(test_resident_task_message_report PRIVATE hanmo_resident)
target_compile_options(test_resident_task_message_report PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
