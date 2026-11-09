# 世界内核与共享事务消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 动态域必要断言使用同一生产库；联合入口需显式正式目录，缺输入不能冒充通过。
foreach(name dynamic_economy dynamic_affairs dynamic_director dynamic_world)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/${name}_test.cpp")
        add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
        target_include_directories(test_resident_${name} PRIVATE "${HANMO_PICOSHA2_ROOT}")
        target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
        target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
        add_test(NAME resident_${name} COMMAND test_resident_${name})
    endif()
endforeach()
