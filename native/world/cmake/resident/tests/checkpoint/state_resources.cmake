# 检查点与延续消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
foreach(name state_resources boundary_codec_test boundary_checkpoint_test boundary_view_test)
    add_executable(test_resident_${name} tests/resident/${name}.cpp)
    target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_resident_${name} PRIVATE -Wall -Wextra -Werror -UNDEBUG -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME resident_${name} COMMAND test_resident_${name})
endforeach()
