# 政治与治理消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 政治签受及真实军官交接仅做具名组件，完整World另核。
foreach(case IN ITEMS political_military_appointments political_military_assignment_joint political_military_appointment_director)
    add_executable(test_resident_${case} tests/resident/${case}_test.cpp)
    target_link_libraries(test_resident_${case} PRIVATE hanmo_resident)
    target_include_directories(test_resident_${case} PRIVATE src)
    target_compile_options(test_resident_${case} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
endforeach()
