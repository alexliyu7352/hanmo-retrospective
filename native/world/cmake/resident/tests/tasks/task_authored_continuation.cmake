# 任务与履约消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 已受领送信与护送只生成原有命令；组件不替代实际NPC履约World。
add_executable(test_resident_task_authored_continuation tests/resident/task_authored_continuation_test.cpp)
target_link_libraries(test_resident_task_authored_continuation PRIVATE hanmo_resident)
target_include_directories(test_resident_task_authored_continuation PRIVATE src)
target_compile_options(test_resident_task_authored_continuation PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)

# 静态实物只核原资源搬运与历史免责的来源闭包，不代完整World。
foreach(case IN ITEMS task_static_asset task_inheritance_history)
    add_executable(test_resident_${case} tests/resident/${case}_test.cpp)
    target_link_libraries(test_resident_${case} PRIVATE hanmo_resident)
    target_include_directories(test_resident_${case} PRIVATE src)
    target_compile_options(test_resident_${case} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
