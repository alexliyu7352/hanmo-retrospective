# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 亲历死亡只读来源，不授通知、财产或控制权。
add_executable(test_resident_military_death_witness tests/resident/military_death_witness_test.cpp)
target_link_libraries(test_resident_military_death_witness PRIVATE hanmo_resident)
target_compile_options(test_resident_military_death_witness PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
