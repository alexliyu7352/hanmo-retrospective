# 事务与需求：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/affairs/native_state_need_world_scope.cpp。
set(HANMO_RESIDENT_SOURCES_affairs_native_state_need_world_scope_cpp
    src/resident/domains/affairs/native_state_need_world_scope.cpp
)

# 生产输入连续登记段：src/resident/domains/affairs/affairs_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_affairs_affairs_checkpoint_cpp
    src/resident/domains/affairs/affairs_checkpoint.cpp
    src/resident/domains/affairs/affairs_codec.cpp
    src/resident/domains/affairs/affairs_rules.cpp
    src/resident/domains/affairs/objective_result_consumer.cpp
    src/resident/domains/affairs/affairs_validation.cpp
    src/resident/domains/affairs/affairs_views.cpp
)

# 生产输入连续登记段：src/resident/domains/affairs/affairs_policy_validation.cpp。
set(HANMO_RESIDENT_SOURCES_affairs_affairs_policy_validation_cpp
    src/resident/domains/affairs/affairs_policy_validation.cpp
    src/resident/domains/affairs/issued_supply/affairs_issued_supply.cpp
    src/resident/domains/affairs/issued_supply/affairs_issued_supply_integration.cpp
)

# 生产输入连续登记段：src/resident/domains/affairs/affairs_calendar_phase.cpp。
set(HANMO_RESIDENT_SOURCES_affairs_affairs_calendar_phase_cpp
    src/resident/domains/affairs/affairs_calendar_phase.cpp
    src/resident/domains/affairs/quantity_contribution_time2.cpp
    src/resident/domains/affairs/state_need.cpp
    src/resident/domains/affairs/state_need_codec.cpp
    src/resident/domains/affairs/state_need_exact_phase.cpp
    src/resident/domains/affairs/state_need_period.cpp
)

# 生产输入连续登记段：src/resident/domains/affairs/state_need_catalog.cpp。
set(HANMO_RESIDENT_SOURCES_affairs_state_need_catalog_cpp
    # Fixed K6 0219 original Time2/Life owner sources and shared Fixed period2.
    src/resident/domains/affairs/state_need_catalog.cpp
    src/resident/domains/affairs/state_need_policy_read.cpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/native_state_need_world_scope.cpp。
set(HANMO_RESIDENT_IDENTITY_affairs_native_state_need_world_scope_cpp
    src/resident/domains/affairs/native_state_need_world_scope.cpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/native_state_need_world_scope.hpp。
set(HANMO_RESIDENT_IDENTITY_affairs_native_state_need_world_scope_hpp
    src/resident/domains/affairs/native_state_need_world_scope.hpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/affairs_supply_suspension.hpp。
set(HANMO_RESIDENT_IDENTITY_affairs_affairs_supply_suspension_hpp
    src/resident/domains/affairs/affairs_supply_suspension.hpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/affairs_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_affairs_affairs_checkpoint_cpp
    src/resident/domains/affairs/affairs_checkpoint.cpp
    src/resident/domains/affairs/affairs_codec.cpp
    src/resident/domains/affairs/affairs_codec.hpp
    src/resident/domains/affairs/affairs_model.hpp
    src/resident/domains/affairs/affairs_rules.cpp
    src/resident/domains/affairs/objective_result_consumer.cpp
    src/resident/domains/affairs/objective_result_consumer.hpp
    src/resident/domains/affairs/affairs_validation.cpp
    src/resident/domains/affairs/affairs_views.cpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/affairs_policy_validation.hpp。
set(HANMO_RESIDENT_IDENTITY_affairs_affairs_policy_validation_hpp
    src/resident/domains/affairs/affairs_policy_validation.hpp
    src/resident/domains/affairs/issued_supply/affairs_issued_supply.hpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/affairs_policy_validation.cpp。
set(HANMO_RESIDENT_IDENTITY_affairs_affairs_policy_validation_cpp
    src/resident/domains/affairs/affairs_policy_validation.cpp
    src/resident/domains/affairs/issued_supply/affairs_issued_supply.cpp
    src/resident/domains/affairs/issued_supply/affairs_issued_supply_integration.cpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/affairs_calendar_phase.cpp。
set(HANMO_RESIDENT_IDENTITY_affairs_affairs_calendar_phase_cpp
    src/resident/domains/affairs/affairs_calendar_phase.cpp
    src/resident/domains/affairs/quantity_contribution_time2.cpp
    src/resident/domains/affairs/state_need.cpp
    src/resident/domains/affairs/state_need_codec.cpp
    src/resident/domains/affairs/state_need_exact_phase.cpp
    src/resident/domains/affairs/state_need_period.cpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/state_need_catalog.cpp。
set(HANMO_RESIDENT_IDENTITY_affairs_state_need_catalog_cpp
    # Fixed K6 0219 original Time2/Life owner sources and shared Fixed period2.
    src/resident/domains/affairs/state_need_catalog.cpp
    src/resident/domains/affairs/state_need_catalog.hpp
    src/resident/domains/affairs/state_need_policy_read.cpp
    src/resident/domains/affairs/state_need_policy_read.hpp
)

# 生产身份输入连续登记段：src/resident/domains/affairs/state_need.hpp。
set(HANMO_RESIDENT_IDENTITY_affairs_state_need_hpp
    src/resident/domains/affairs/state_need.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/affairs/source_owned_fields/domain_fields.inc
    src/resident/domains/affairs/world_codec/catalog_needs.inc
    src/resident/domains/affairs/world_codec/checkpoint_read_affairs.inc
    src/resident/domains/affairs/world_codec/checkpoint_write_original.inc
    src/resident/domains/affairs/world_codec/seed_original.inc
    src/resident/domains/affairs/world_codec/selection_needs.inc
)
