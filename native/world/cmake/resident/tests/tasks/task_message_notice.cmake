# 任务与履约消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 实体死亡信件只沿原Task与精确来源进入，新旧冷段分开。
foreach(case IN ITEMS task_message_notice task_message_notice_catalog)
    add_executable(test_resident_${case} tests/resident/${case}_test.cpp)
    target_link_libraries(test_resident_${case} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${case} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
