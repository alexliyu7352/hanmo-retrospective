# 组织与成员消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
add_executable(test_resident_organization_contract tests/resident/organization_contract_test.cpp)
target_include_directories(test_resident_organization_contract PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_organization_contract PRIVATE hanmo_resident)
target_compile_options(test_resident_organization_contract PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
