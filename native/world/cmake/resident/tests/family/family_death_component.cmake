# 家庭与生命阶段消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 新生命段的必要原因、冷边界和事务门；真实军事/海军过程由同代World另验。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/family_death_component_test.cpp")
    add_executable(test_resident_family_death_component tests/resident/family_death_component_test.cpp)
    target_include_directories(test_resident_family_death_component PRIVATE src)
    target_link_libraries(test_resident_family_death_component PRIVATE hanmo_resident)
    target_compile_options(test_resident_family_death_component PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
    add_test(NAME resident_family_death_component COMMAND test_resident_family_death_component)
endif()

# 寿终桥同候选来源与时钟绑定组件；不初始化世界，不代表 LIFE-5 业务验收。
add_executable(test_resident_life_age_condition_sources tests/resident/life_age_condition_sources_test.cpp)
target_include_directories(test_resident_life_age_condition_sources PRIVATE src)
target_link_libraries(test_resident_life_age_condition_sources PRIVATE hanmo_resident)
target_compile_options(test_resident_life_age_condition_sources PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME resident_life_age_condition_sources COMMAND test_resident_life_age_condition_sources)

# 条件控制许可的必要持久化与事务门，不代完整World/GUI验收。
foreach(name IN ITEMS family_legacy family_legacy_receipt legacy_control_consent family_bequest family_bequest_source family_inheritance bequest_disclosure_source inheritance_personal_assets family_marriage marriage_source)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/${name}_test.cpp")
        add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
        target_include_directories(test_resident_${name} PRIVATE src)
        target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
        target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
        add_test(NAME resident_${name} COMMAND test_resident_${name})
    endif()
endforeach()
