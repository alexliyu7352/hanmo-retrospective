# 地方经济与责任账消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 新军籍来源和人口守恒仅在本轮联合节点显式执行，使用同一生产库及开启的断言。
foreach(name population_military_economy population_military_source)
    add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
    target_include_directories(test_resident_${name} PRIVATE "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
    add_test(NAME resident_${name} COMMAND test_resident_${name})
endforeach()
