# 制造与修缮：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/craft/repair_spatial_sources.cpp。
set(HANMO_RESIDENT_SOURCES_craft_repair_spatial_sources_cpp
    src/resident/domains/craft/repair_spatial_sources.cpp
    src/resident/domains/craft/craft_spatial_sources.cpp
)

# 生产输入连续登记段：src/resident/domains/craft/item_service.cpp。
set(HANMO_RESIDENT_SOURCES_craft_item_service_cpp
    src/resident/domains/craft/item_service.cpp
    src/resident/domains/craft/item_service_codec.cpp
    src/resident/domains/craft/item_service_initial.cpp
    src/resident/domains/craft/repair_codec.cpp
)

# 生产输入连续登记段：src/resident/domains/craft/craft_action_time.cpp。
set(HANMO_RESIDENT_SOURCES_craft_craft_action_time_cpp
    # 原 Craft 服务与候选区间：只登记源码，不替完整作者/工厂准入。
    src/resident/domains/craft/craft_action_time.cpp
    src/resident/domains/craft/craft_control.cpp
    src/resident/domains/craft/craft_exact_phase.cpp
    src/resident/domains/craft/craft_interval.cpp
    src/resident/domains/craft/craft_job.cpp
    src/resident/domains/craft/craft_job_codec.cpp
    src/resident/domains/craft/craft_materials.cpp
    src/resident/domains/craft/craft_output.cpp
    src/resident/domains/craft/craft_pace.cpp
    src/resident/domains/craft/craft_quality.cpp
    src/resident/domains/craft/craft_recipe.cpp
    src/resident/domains/craft/craft_recipe_codec.cpp
)

# 生产输入连续登记段：src/resident/domains/craft/craft_cancellation.cpp。
set(HANMO_RESIDENT_SOURCES_craft_craft_cancellation_cpp
    src/resident/domains/craft/craft_cancellation.cpp
    src/resident/domains/craft/craft_character_sources.cpp
    src/resident/domains/craft/craft_condition_effects.cpp
    src/resident/domains/craft/craft_facility_boundary_read.cpp
    src/resident/domains/craft/craft_facility_boundary_write.cpp
    src/resident/domains/craft/craft_facility_occupancy.cpp
    src/resident/domains/craft/craft_facility_runtime.cpp
    src/resident/domains/craft/craft_facility_sources.cpp
    src/resident/domains/craft/item_condition_contract.cpp
    src/resident/domains/craft/item_service_catalog.cpp
    src/resident/domains/craft/repair_acceptance.cpp
    src/resident/domains/craft/repair_action_time.cpp
    src/resident/domains/craft/repair_character_sources.cpp
    src/resident/domains/craft/repair_completion.cpp
    src/resident/domains/craft/repair_contract.cpp
    src/resident/domains/craft/repair_control.cpp
    src/resident/domains/craft/repair_exact_phase.cpp
    src/resident/domains/craft/repair_policy.cpp
    src/resident/domains/craft/repair_service_sources.cpp
    src/resident/domains/craft/repair_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/craft/repair_spatial_sources.cpp。
set(HANMO_RESIDENT_IDENTITY_craft_repair_spatial_sources_cpp
    src/resident/domains/craft/repair_spatial_sources.cpp
    src/resident/domains/craft/craft_spatial_sources.cpp
)

# 生产身份输入连续登记段：src/resident/domains/craft/item_service.cpp。
set(HANMO_RESIDENT_IDENTITY_craft_item_service_cpp
    src/resident/domains/craft/item_service.cpp
    src/resident/domains/craft/item_service_codec.cpp
    src/resident/domains/craft/item_service_initial.cpp
    src/resident/domains/craft/repair_codec.cpp
)

# 生产身份输入连续登记段：src/resident/domains/craft/craft_action_time.cpp。
set(HANMO_RESIDENT_IDENTITY_craft_craft_action_time_cpp
    src/resident/domains/craft/craft_action_time.cpp
    src/resident/domains/craft/craft_action_time.hpp
    src/resident/domains/craft/craft_codec_archive.hpp
    src/resident/domains/craft/craft_control.cpp
    src/resident/domains/craft/craft_control.hpp
    src/resident/domains/craft/craft_exact_phase.cpp
    src/resident/domains/craft/craft_exact_phase.hpp
    src/resident/domains/craft/craft_interval.cpp
    src/resident/domains/craft/craft_interval.hpp
    src/resident/domains/craft/craft_job.cpp
    src/resident/domains/craft/craft_job.hpp
    src/resident/domains/craft/craft_job_codec.cpp
    src/resident/domains/craft/craft_job_codec.hpp
    src/resident/domains/craft/craft_job_fields.hpp
    src/resident/domains/craft/craft_job_model.hpp
    src/resident/domains/craft/craft_materials.cpp
    src/resident/domains/craft/craft_materials.hpp
    src/resident/domains/craft/craft_output.cpp
    src/resident/domains/craft/craft_output.hpp
    src/resident/domains/craft/craft_pace.cpp
    src/resident/domains/craft/craft_pace.hpp
    src/resident/domains/craft/craft_quality.cpp
    src/resident/domains/craft/craft_quality.hpp
    src/resident/domains/craft/craft_recipe.cpp
    src/resident/domains/craft/craft_recipe.hpp
    src/resident/domains/craft/craft_recipe_codec.cpp
    src/resident/domains/craft/craft_recipe_fields.hpp
)

# 生产身份输入连续登记段：src/resident/domains/craft/craft_cancellation.cpp。
set(HANMO_RESIDENT_IDENTITY_craft_craft_cancellation_cpp
    src/resident/domains/craft/craft_cancellation.cpp
    src/resident/domains/craft/craft_character_sources.cpp
    src/resident/domains/craft/craft_condition_effects.cpp
    src/resident/domains/craft/craft_facility_boundary_read.cpp
    src/resident/domains/craft/craft_facility_boundary_write.cpp
    src/resident/domains/craft/craft_facility_occupancy.cpp
    src/resident/domains/craft/craft_facility_runtime.cpp
    src/resident/domains/craft/craft_facility_sources.cpp
    src/resident/domains/craft/item_condition_contract.cpp
    src/resident/domains/craft/item_service_catalog.cpp
    src/resident/domains/craft/repair_acceptance.cpp
    src/resident/domains/craft/repair_action_time.cpp
    src/resident/domains/craft/repair_character_sources.cpp
    src/resident/domains/craft/repair_completion.cpp
    src/resident/domains/craft/repair_contract.cpp
    src/resident/domains/craft/repair_control.cpp
    src/resident/domains/craft/repair_exact_phase.cpp
    src/resident/domains/craft/repair_policy.cpp
    src/resident/domains/craft/repair_service_sources.cpp
    src/resident/domains/craft/repair_validation.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/craft/source_owned_fields/domain_fields.inc
    src/resident/domains/craft/world_codec/checkpoint_read_item_instances.inc
    src/resident/domains/craft/world_codec/checkpoint_write_item_craft.inc
    src/resident/domains/craft/world_codec/export_item_craft_payloads.inc
    src/resident/domains/craft/world_codec/export_item_craft_state.inc
    src/resident/domains/craft/world_codec/export_validate_original_headers.inc
    src/resident/domains/craft/world_codec/seed_item_craft.inc
)
