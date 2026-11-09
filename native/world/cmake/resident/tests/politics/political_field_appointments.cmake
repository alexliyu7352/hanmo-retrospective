# 政治与治理消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 野外任命只跑具名权源和两步同候选门，真实战役运行另验。
foreach(case IN ITEMS political_field_appointments political_field_joint)
    add_executable(test_resident_${case} tests/resident/${case}_test.cpp)
    target_link_libraries(test_resident_${case} PRIVATE hanmo_resident)
    target_include_directories(test_resident_${case} PRIVATE src)
    target_compile_options(test_resident_${case} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
target_compile_options(test_resident_military_field_command PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
target_compile_options(test_resident_military_field_codec PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
