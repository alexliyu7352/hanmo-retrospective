# 检查点与延续消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 有限继续只注册本批边界与资金消费者，断言不可随发行优化关闭。
foreach(test_name IN ITEMS continuation_snapshot continuation_binding wallet_debit_observation kernel_bounded_run)
    add_executable(test_resident_${test_name} tests/resident/${test_name}_test.cpp)
    target_include_directories(test_resident_${test_name} PRIVATE "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(test_resident_${test_name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${test_name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
