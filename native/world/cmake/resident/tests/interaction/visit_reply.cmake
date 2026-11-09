# 交互与对白消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 原拜帖领读/本人回信/实领、冷历史与永久结果门；运行单独按风险排期。
add_executable(test_resident_visit_reply tests/resident/visit_reply_test.cpp)
target_link_libraries(test_resident_visit_reply PRIVATE hanmo_resident)
target_include_directories(test_resident_visit_reply PRIVATE src)
target_compile_options(test_resident_visit_reply PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME resident_visit_reply COMMAND test_resident_visit_reply)
