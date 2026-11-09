# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 请求门使用同代完整核心，不与旧库混合。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_request_policy_test.cpp")
    add_executable(test_resident_military_request_policy tests/resident/military_request_policy_test.cpp)
    target_include_directories(test_resident_military_request_policy PRIVATE src)
    target_link_libraries(test_resident_military_request_policy PRIVATE hanmo_resident)
    add_test(NAME resident_military_request_policy COMMAND test_resident_military_request_policy)
endif()

# 实船有限门与生产同一核心，不能替代真正World/Main舟行验证。
foreach(name naval_repair naval_assets naval_asset_codec naval_authoring naval_command_codec naval_commands naval_exit naval_source_validation naval_travel_proof naval_views naval_world naval_schedule naval_release_authority naval_location_source naval_journey_source naval_commander_loss military_actor_occupation military_commander_loss)
    add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
    target_include_directories(test_resident_${name} PRIVATE src)
    target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${name} PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
    add_test(NAME resident_${name} COMMAND test_resident_${name})
endforeach()
