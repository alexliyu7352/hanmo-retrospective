# 检查点与延续消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
add_executable(test_resident_boundary_receipt tests/resident/boundary_receipt_test.cpp)
target_link_libraries(test_resident_boundary_receipt PRIVATE hanmo_resident)
target_compile_options(test_resident_boundary_receipt PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
