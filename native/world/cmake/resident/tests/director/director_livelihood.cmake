# 自主决策消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 本批只登记已存在的定向消费者；联合作者消费者需要显式正式目录，不在配置时执行。
foreach(name director_livelihood regional_life_evidence regional_life_world regional_life_interaction)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/${name}_test.cpp")
        add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
        target_include_directories(test_resident_${name} PRIVATE "${HANMO_PICOSHA2_ROOT}")
        target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
        target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
    endif()
endforeach()
