# 政治与治理消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 双签通行本域来源、原两次占领与条件/回执边界。
foreach(case IN ITEMS treaty_passage_rules treaty_passage_boundary treaty_passage_control_history)
    add_executable(test_resident_${case} tests/resident/${case}_test.cpp)
    target_link_libraries(test_resident_${case} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${case} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
