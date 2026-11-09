# 社交与传播：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/social/social_original_result_projection.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_original_result_projection_cpp
    src/resident/domains/social/social_original_result_projection.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_event_original_source.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_event_original_source_cpp
    src/resident/domains/social/social_event_original_source.cpp
    src/resident/domains/social/social_event_application_codec.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_relation_state_codec.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_relation_state_codec_cpp
    src/resident/domains/social/social_relation_state_codec.cpp
    src/resident/domains/social/social_relation_retention.cpp
    src/resident/domains/social/social_relation_result_codec.cpp
    src/resident/domains/social/social_relation_result.cpp
    src/resident/domains/social/social_relation_original_sources.cpp
    src/resident/domains/social/social_relation_execution.cpp
    src/resident/domains/social/propagation_adoption_memory.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_original_policy.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_original_policy_cpp
    src/resident/domains/social/social_original_policy.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_time2.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_time2_cpp
    src/resident/domains/social/social_time2.cpp
    src/resident/domains/social/social_period_sources.cpp
    src/resident/domains/social/social_time2_codec.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_dynamic_codec.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_dynamic_codec_cpp
    src/resident/domains/social/social_dynamic_codec.cpp
    src/resident/domains/social/social_dynamic_rules.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_codec.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_codec_cpp
    src/resident/domains/social/social_codec.cpp
    src/resident/domains/social/social_rules.cpp
)

# 生产输入连续登记段：src/resident/domains/social/knowledge_read.cpp。
set(HANMO_RESIDENT_SOURCES_social_knowledge_read_cpp
    src/resident/domains/social/knowledge_read.cpp
    src/resident/domains/social/knowledge_retention.cpp
    src/resident/domains/social/known_supply_read2.cpp
    src/resident/domains/social/propagation.cpp
    src/resident/domains/social/propagation_policy.cpp
    src/resident/domains/social/social_exact_collection.cpp
    src/resident/domains/social/social_exact_phase.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_knowledge_cut.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_knowledge_cut_cpp
    src/resident/domains/social/social_knowledge_cut.cpp
)

# 生产输入连续登记段：src/resident/domains/social/knowledge_report_category.cpp。
set(HANMO_RESIDENT_SOURCES_social_knowledge_report_category_cpp
    src/resident/domains/social/knowledge_report_category.cpp
)

# 生产输入连续登记段：src/resident/domains/social/actor_report_knowledge.cpp。
set(HANMO_RESIDENT_SOURCES_social_actor_report_knowledge_cpp
    src/resident/domains/social/actor_report_knowledge.cpp
    src/resident/domains/social/known_death_receipt.cpp
)

# 生产输入连续登记段：src/resident/domains/social/social_direct_effect_read.cpp。
set(HANMO_RESIDENT_SOURCES_social_social_direct_effect_read_cpp
    src/resident/domains/social/social_direct_effect_read.cpp
    src/resident/domains/social/social_response_rules.cpp
    src/resident/domains/social/social_window_codec.cpp
    src/resident/domains/social/social_window_model.cpp
    src/resident/domains/social/social_window_rules.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_original_result_projection.hpp。
set(HANMO_RESIDENT_IDENTITY_social_social_original_result_projection_hpp
    src/resident/domains/social/social_original_result_projection.hpp
    src/resident/domains/social/social_original_result_projection.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_event_original_source.hpp。
set(HANMO_RESIDENT_IDENTITY_social_social_event_original_source_hpp
    src/resident/domains/social/social_event_original_source.hpp
    src/resident/domains/social/social_event_original_source.cpp
    src/resident/domains/social/social_event_application_codec_fields.hpp
    src/resident/domains/social/social_event_application_codec.hpp
    src/resident/domains/social/social_event_application_codec.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_relation_state_codec.hpp。
set(HANMO_RESIDENT_IDENTITY_social_social_relation_state_codec_hpp
    src/resident/domains/social/social_relation_state_codec.hpp
    src/resident/domains/social/social_relation_state_codec.cpp
    src/resident/domains/social/social_relation_retention.hpp
    src/resident/domains/social/social_relation_retention.cpp
    src/resident/domains/social/social_relation_result_codec_fields.hpp
    src/resident/domains/social/social_relation_result_codec.hpp
    src/resident/domains/social/social_relation_result_codec.cpp
    src/resident/domains/social/social_relation_result.hpp
    src/resident/domains/social/social_relation_result.cpp
    src/resident/domains/social/social_relation_original_sources.hpp
    src/resident/domains/social/social_relation_original_sources.cpp
    src/resident/domains/social/social_relation_execution.hpp
    src/resident/domains/social/social_relation_execution.cpp
    src/resident/domains/social/social_relation_cut.hpp
    src/resident/domains/social/propagation_adoption_memory.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_original_policy.hpp。
set(HANMO_RESIDENT_IDENTITY_social_social_original_policy_hpp
    src/resident/domains/social/social_original_policy.hpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_original_policy.cpp。
set(HANMO_RESIDENT_IDENTITY_social_social_original_policy_cpp
    src/resident/domains/social/social_original_policy.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_period_sources.cpp。
set(HANMO_RESIDENT_IDENTITY_social_social_period_sources_cpp
    src/resident/domains/social/social_period_sources.cpp
    src/resident/domains/social/social_period_sources.hpp
    src/resident/domains/social/social_time2.cpp
    src/resident/domains/social/social_time2.hpp
    src/resident/domains/social/social_time2_codec.cpp
    src/resident/domains/social/social_time2_codec.hpp
    src/resident/domains/social/social_time2_model.hpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_dynamic_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_social_social_dynamic_codec_cpp
    src/resident/domains/social/social_dynamic_codec.cpp
    src/resident/domains/social/social_dynamic_rules.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_social_social_codec_cpp
    src/resident/domains/social/social_codec.cpp
    src/resident/domains/social/social_model.hpp
    src/resident/domains/social/social_rules.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/knowledge_read.cpp。
set(HANMO_RESIDENT_IDENTITY_social_knowledge_read_cpp
    src/resident/domains/social/knowledge_read.cpp
    src/resident/domains/social/knowledge_report_category.cpp
    src/resident/domains/social/knowledge_retention.cpp
    src/resident/domains/social/known_supply_read2.cpp
    src/resident/domains/social/propagation.cpp
    src/resident/domains/social/propagation_policy.cpp
    src/resident/domains/social/social_exact_collection.cpp
    src/resident/domains/social/social_exact_phase.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_knowledge_cut.cpp。
set(HANMO_RESIDENT_IDENTITY_social_social_knowledge_cut_cpp
    src/resident/domains/social/social_knowledge_cut.cpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_knowledge_cut.hpp。
set(HANMO_RESIDENT_IDENTITY_social_social_knowledge_cut_hpp
    src/resident/domains/social/social_knowledge_cut.hpp
    src/resident/domains/social/social_time2_version.hpp
    src/resident/domains/social/social_knowledge_key.hpp
    src/resident/domains/social/propagation_model.hpp
    src/resident/domains/social/propagation_disposition_read.hpp
)

# 生产身份输入连续登记段：src/resident/domains/social/actor_report_knowledge.cpp。
set(HANMO_RESIDENT_IDENTITY_social_actor_report_knowledge_cpp
    src/resident/domains/social/actor_report_knowledge.cpp
    src/resident/domains/social/actor_report_knowledge.hpp
    src/resident/domains/social/knowledge_category.hpp
    src/resident/domains/social/knowledge_read.hpp
    src/resident/domains/social/knowledge_report_category.hpp
    src/resident/domains/social/known_death_receipt.cpp
    src/resident/domains/social/known_death_receipt.hpp
    src/resident/domains/social/known_supply_read2.hpp
    src/resident/domains/social/propagation.hpp
    src/resident/domains/social/propagation_policy.hpp
    src/resident/domains/social/social_dynamic_json_read.hpp
)

# 生产身份输入连续登记段：src/resident/domains/social/social_direct_effect_read.cpp。
set(HANMO_RESIDENT_IDENTITY_social_social_direct_effect_read_cpp
    src/resident/domains/social/social_direct_effect_read.cpp
    src/resident/domains/social/social_response_rules.cpp
    src/resident/domains/social/social_window_codec.cpp
    src/resident/domains/social/social_window_model.cpp
    src/resident/domains/social/social_window_rules.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/social/source_owned_fields/domain_fields.inc
    src/resident/domains/social/world_codec/catalog_original.inc
    src/resident/domains/social/world_codec/checkpoint_read_dynamic_social.inc
    src/resident/domains/social/world_codec/checkpoint_read_social.inc
    src/resident/domains/social/world_codec/checkpoint_write_dynamic.inc
    src/resident/domains/social/world_codec/seed_original.inc
)
