# 人物与控制底座消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# Teaching proofs and permanent codec consume this same ABI; full World drivers remain explicit.
foreach(name household_control character_creation_source growth_teaching_requests growth_teaching_requests_codec growth_insight_sources growth_insight_cards growth_study_teaching_requests growth_study_teaching_codec growth_study_teaching_lifecycle)
    add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
    target_include_directories(test_resident_${name} PRIVATE src)
    target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
    add_test(NAME resident_${name} COMMAND test_resident_${name})
endforeach()
