# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 协商整编有限来源消费者只在本轮组合节点显式运行。
foreach(name siege_integration_decisions military_integration_assets siege_integration_boundary)
    add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
    target_include_directories(test_resident_${name} PRIVATE "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
