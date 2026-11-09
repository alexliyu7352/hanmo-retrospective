# 成长与教学消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 普通课程逐半日结算与原题迟答的有限消费者；必须启用断言。
add_executable(test_resident_study_segments tests/resident/study_segments_test.cpp)
target_include_directories(test_resident_study_segments PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_study_segments PRIVATE hanmo_resident)
target_compile_options(test_resident_study_segments PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
