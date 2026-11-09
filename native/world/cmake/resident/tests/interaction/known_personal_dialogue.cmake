# 交互与对白消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# Real known-personal origins, original session, permanent actual result; runtime gates deferred.
add_executable(test_resident_known_personal_dialogue tests/resident/known_personal_dialogue_source_test.cpp)
target_link_libraries(test_resident_known_personal_dialogue PRIVATE hanmo_resident)
target_include_directories(test_resident_known_personal_dialogue PRIVATE src)
target_compile_options(test_resident_known_personal_dialogue PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME resident_known_personal_dialogue COMMAND test_resident_known_personal_dialogue)
