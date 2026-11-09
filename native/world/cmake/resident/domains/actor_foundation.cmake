# 人物与控制底座：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/actor_foundation/actor_foundation_codec.cpp。
set(HANMO_RESIDENT_SOURCES_actor_foundation_actor_foundation_codec_cpp
    src/resident/domains/actor_foundation/actor_foundation_codec.cpp
    src/resident/domains/actor_foundation/authored_catalog.cpp
    src/resident/domains/actor_foundation/authored_origin.cpp
    src/resident/domains/actor_foundation/rest_condition.cpp
)

# 生产输入连续登记段：src/resident/domains/actor_foundation/character_capability.cpp。
set(HANMO_RESIDENT_SOURCES_actor_foundation_character_capability_cpp
    src/resident/domains/actor_foundation/character_capability.cpp
    src/resident/domains/actor_foundation/character_condition_read.cpp
    src/resident/domains/actor_foundation/character_condition_world.cpp
    src/resident/domains/actor_foundation/character_expression.cpp
    src/resident/domains/actor_foundation/character_presence_v2.cpp
    src/resident/domains/actor_foundation/condition_capture.cpp
    src/resident/domains/actor_foundation/condition_application_fact.cpp
    src/resident/domains/actor_foundation/condition_source_read.cpp
    src/resident/domains/actor_foundation/condition_source_retention.cpp
    src/resident/domains/actor_foundation/created_origin.cpp
    src/resident/domains/actor_foundation/life_condition.cpp
    src/resident/domains/actor_foundation/long_rest_condition.cpp
    src/resident/domains/actor_foundation/military_condition.cpp
    src/resident/domains/actor_foundation/travel_condition.cpp
    src/resident/domains/actor_foundation/work_condition.cpp
)

# 生产输入连续登记段：src/resident/domains/actor_foundation/military_availability.cpp。
set(HANMO_RESIDENT_SOURCES_actor_foundation_military_availability_cpp
    # Fixed actual selected readers, source capture, and original owning codecs.
    src/resident/domains/actor_foundation/military_availability.cpp
    src/resident/domains/actor_foundation/service_condition.cpp
)

# 生产输入连续登记段：src/resident/domains/actor_foundation/injury_attribute_reducer.cpp。
set(HANMO_RESIDENT_SOURCES_actor_foundation_injury_attribute_reducer_cpp
    src/resident/domains/actor_foundation/injury_attribute_reducer.cpp
)

# 生产输入连续登记段：src/resident/domains/actor_foundation/born_origin.cpp。
set(HANMO_RESIDENT_SOURCES_actor_foundation_born_origin_cpp
    # 完整生产编译补齐既有领域实现；含 main 的独立测试不混入生产库。
    src/resident/domains/actor_foundation/born_origin.cpp
    src/resident/domains/actor_foundation/character_availability_read.cpp
    src/resident/domains/actor_foundation/character_origin_validation.cpp
    src/resident/domains/actor_foundation/condition_capacity.cpp
    src/resident/domains/actor_foundation/craft_condition.cpp
    src/resident/domains/actor_foundation/injury_capability_map.cpp
    src/resident/domains/actor_foundation/military_supply_condition.cpp
    src/resident/domains/actor_foundation/native_field_supply_condition.cpp
)

# 生产身份输入连续登记段：src/resident/domains/actor_foundation/actor_foundation_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_actor_foundation_actor_foundation_codec_cpp
    src/resident/domains/actor_foundation/actor_foundation_codec.cpp
    src/resident/domains/actor_foundation/actor_foundation_codec.hpp
    src/resident/domains/actor_foundation/actor_foundation_scoped_codec.hpp
    src/resident/domains/actor_foundation/frozen_condition_codec_fields.hpp
    src/resident/domains/actor_foundation/actor_foundation_json_visitor.hpp
    src/resident/domains/actor_foundation/authored_catalog.cpp
    src/resident/domains/actor_foundation/authored_origin.cpp
    src/resident/domains/actor_foundation/authored_origin.hpp
    src/resident/domains/actor_foundation/validated_catalog.hpp
    src/resident/domains/actor_foundation/model.hpp
    src/resident/domains/actor_foundation/rest_condition.cpp
    src/resident/domains/actor_foundation/rest_condition.hpp
)

# 生产身份输入连续登记段：src/resident/domains/actor_foundation/character_capability.cpp。
set(HANMO_RESIDENT_IDENTITY_actor_foundation_character_capability_cpp
    src/resident/domains/actor_foundation/character_capability.cpp
    src/resident/domains/actor_foundation/family_personal_capability.hpp
    src/resident/domains/actor_foundation/facility_construction_capability.hpp
    src/resident/domains/actor_foundation/character_condition_read.cpp
    src/resident/domains/actor_foundation/character_condition_world.cpp
    src/resident/domains/actor_foundation/character_expression.cpp
    src/resident/domains/actor_foundation/character_presence_v2.cpp
    src/resident/domains/actor_foundation/condition_capture.cpp
    src/resident/domains/actor_foundation/condition_application_fact.cpp
    src/resident/domains/actor_foundation/condition_application_fact.hpp
    src/resident/domains/actor_foundation/condition_application_history.hpp
    src/resident/domains/actor_foundation/family_personal_capability_history.hpp
    src/resident/domains/actor_foundation/family_communication_capability.hpp
    src/resident/domains/actor_foundation/condition_source_read.cpp
    src/resident/domains/actor_foundation/condition_source_retention.cpp
    src/resident/domains/actor_foundation/created_origin.cpp
    src/resident/domains/actor_foundation/life_condition.cpp
    src/resident/domains/actor_foundation/long_rest_condition.cpp
    src/resident/domains/actor_foundation/military_availability.cpp
    src/resident/domains/actor_foundation/military_condition.cpp
    src/resident/domains/actor_foundation/service_condition.cpp
    src/resident/domains/actor_foundation/travel_condition.cpp
    src/resident/domains/actor_foundation/work_condition.cpp
)

# 生产身份输入连续登记段：src/resident/domains/actor_foundation/injury_attribute_reducer.cpp。
set(HANMO_RESIDENT_IDENTITY_actor_foundation_injury_attribute_reducer_cpp
    src/resident/domains/actor_foundation/injury_attribute_reducer.cpp
)

# 生产身份输入连续登记段：src/resident/domains/actor_foundation/born_origin.cpp。
set(HANMO_RESIDENT_IDENTITY_actor_foundation_born_origin_cpp
    src/resident/domains/actor_foundation/born_origin.cpp
    src/resident/domains/actor_foundation/character_availability_read.cpp
    src/resident/domains/actor_foundation/character_origin_validation.cpp
    src/resident/domains/actor_foundation/condition_capacity.cpp
    src/resident/domains/actor_foundation/craft_condition.cpp
    src/resident/domains/actor_foundation/injury_capability_map.cpp
    src/resident/domains/actor_foundation/military_supply_condition.cpp
    src/resident/domains/actor_foundation/native_field_supply_condition.cpp
    src/resident/domains/actor_foundation/native_field_supply_condition.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/actor_foundation/source_owned_fields/domain_fields.inc
    src/resident/domains/actor_foundation/world_codec/catalog_authored_origins.inc
    src/resident/domains/actor_foundation/world_codec/checkpoint_read_actor_start_context.inc
    src/resident/domains/actor_foundation/world_codec/checkpoint_read_character_condition_extensions.inc
    src/resident/domains/actor_foundation/world_codec/checkpoint_write_exact_origins.inc
    src/resident/domains/actor_foundation/world_codec/export_exact_origins.inc
    src/resident/domains/actor_foundation/world_codec/seed_exact_origins.inc
)

# H2：轻量原合同只登记身份，不新增生产翻译单元。
list(APPEND HANMO_RESIDENT_IDENTITY_actor_foundation_actor_foundation_codec_cpp
    src/resident/domains/actor_foundation/actor_foundation_refs.hpp
)
