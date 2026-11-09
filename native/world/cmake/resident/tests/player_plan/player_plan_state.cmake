# 玩家计划消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 玩家有限计划只注册本轮领域与真实world消费者，Release同样保留断言与原Pico依赖。
foreach(test_name IN ITEMS player_plan_state player_plan_sources player_plan_execution player_plan_run)
    add_executable(test_resident_${test_name} tests/resident/${test_name}_test.cpp)
    target_include_directories(test_resident_${test_name} PRIVATE "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(test_resident_${test_name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${test_name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
