# 地方经济与责任账消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 征募只读原占领窗口，测试保真实登记前后源及人口/资产守恒。
add_executable(test_resident_population_occupation tests/resident/population_occupation_test.cpp)
target_link_libraries(test_resident_population_occupation PRIVATE hanmo_resident)
target_compile_options(test_resident_population_occupation PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
