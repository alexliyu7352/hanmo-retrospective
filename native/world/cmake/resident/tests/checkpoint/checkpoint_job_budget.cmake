# 检查点与延续消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
add_executable(test_native_checkpoint_job_budget tests/resident/checkpoint_job_budget_test.cpp)
target_link_libraries(test_native_checkpoint_job_budget PRIVATE hanmo_resident)
target_include_directories(test_native_checkpoint_job_budget PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_checkpoint_job_budget PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_checkpoint_job_budget COMMAND test_native_checkpoint_job_budget)
