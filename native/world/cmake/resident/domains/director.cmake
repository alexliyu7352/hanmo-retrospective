# 自主决策：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/director/director_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_director_director_checkpoint_cpp
    src/resident/domains/director/director_checkpoint.cpp
    src/resident/domains/director/director_codec.cpp
    src/resident/domains/director/director_rules.cpp
    src/resident/domains/director/director_registry.cpp
    src/resident/domains/director/director_membership.cpp
    src/resident/domains/director/director_livelihood.cpp
    src/resident/domains/director/director_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/director/director_authority_wake2.cpp。
set(HANMO_RESIDENT_SOURCES_director_director_authority_wake2_cpp
    src/resident/domains/director/director_authority_wake2.cpp
    src/resident/domains/director/director_cohort_purchase2.cpp
    src/resident/domains/director/director_command_acceptance.cpp
    src/resident/domains/director/director_exact_runtime.cpp
    src/resident/domains/director/director_knowledge_wake2.cpp
    src/resident/domains/director/director_subject2.cpp
    src/resident/domains/director/director_subject2_codec.cpp
    src/resident/domains/director/director_task_wake2.cpp
)

# 生产输入连续登记段：src/resident/domains/director/director_actor_proposal2.cpp。
set(HANMO_RESIDENT_SOURCES_director_director_actor_proposal2_cpp
    # Fixed K6 0005 original owner/communication sources and S1 actual completion.
    src/resident/domains/director/director_actor_proposal2.cpp
    src/resident/domains/director/director_actor_read.cpp
    src/resident/domains/director/director_cohort_sources2.cpp
    src/resident/domains/director/director_native_executor2.cpp
    src/resident/domains/director/director_native_subjects2.cpp
    src/resident/domains/director/director_native_wakes2.cpp
)

# 生产输入连续登记段：src/resident/domains/director/director_appointment_native2.cpp。
set(HANMO_RESIDENT_SOURCES_director_director_appointment_native2_cpp
    src/resident/domains/director/director_appointment_native2.cpp
    src/resident/domains/director/director_appointment_wake2.cpp
)

# 生产输入连续登记段：src/resident/domains/director/director_native_lease.cpp。
set(HANMO_RESIDENT_SOURCES_director_director_native_lease_cpp
    src/resident/domains/director/director_native_lease.cpp
)

# 生产身份输入连续登记段：src/resident/domains/director/director_livelihood.hpp。
set(HANMO_RESIDENT_IDENTITY_director_director_livelihood_hpp
    src/resident/domains/director/director_livelihood.hpp
)

# 生产身份输入连续登记段：src/resident/domains/director/director_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_director_director_checkpoint_cpp
    src/resident/domains/director/director_checkpoint.cpp
    src/resident/domains/director/director_codec.cpp
    src/resident/domains/director/director_model.hpp
    src/resident/domains/director/director_rules.cpp
    src/resident/domains/director/director_registry.cpp
    src/resident/domains/director/director_membership.cpp
    src/resident/domains/director/director_livelihood.cpp
    src/resident/domains/director/director_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/director/director_actor_proposal2.cpp。
set(HANMO_RESIDENT_IDENTITY_director_director_actor_proposal2_cpp
    src/resident/domains/director/director_actor_proposal2.cpp
    src/resident/domains/director/director_actor_read.cpp
    src/resident/domains/director/director_authority_wake2.cpp
    src/resident/domains/director/director_cohort_purchase2.cpp
    src/resident/domains/director/director_cohort_sources2.cpp
    src/resident/domains/director/director_command_acceptance.cpp
    src/resident/domains/director/director_exact_runtime.cpp
    src/resident/domains/director/director_knowledge_wake2.cpp
    src/resident/domains/director/director_native_executor2.cpp
    src/resident/domains/director/director_native_subjects2.cpp
    src/resident/domains/director/director_native_wakes2.cpp
    src/resident/domains/director/director_subject2.cpp
    src/resident/domains/director/director_subject2_codec.cpp
    src/resident/domains/director/director_task_wake2.cpp
)

# 生产身份输入连续登记段：src/resident/domains/director/director_actor_proposal2.hpp。
set(HANMO_RESIDENT_IDENTITY_director_director_actor_proposal2_hpp
    src/resident/domains/director/director_actor_proposal2.hpp
    src/resident/domains/director/director_actor_read.hpp
    src/resident/domains/director/director_appointment_native2.cpp
    src/resident/domains/director/director_appointment_native2.hpp
    src/resident/domains/director/director_appointment_wake2.cpp
    src/resident/domains/director/director_appointment_wake2.hpp
    src/resident/domains/director/director_cohort_sources2.hpp
    src/resident/domains/director/director_command_acceptance.hpp
    src/resident/domains/director/director_exact_runtime.hpp
    src/resident/domains/director/director_native_executor2.hpp
    src/resident/domains/director/director_native_subjects2.hpp
    src/resident/domains/director/director_native_wakes2.hpp
    src/resident/domains/director/director_subject2.hpp
    src/resident/domains/director/director_subject2_model.hpp
)

# 生产身份输入连续登记段：src/resident/domains/director/director_native_lease.cpp。
set(HANMO_RESIDENT_IDENTITY_director_director_native_lease_cpp
    src/resident/domains/director/director_native_lease.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/director/source_owned_fields/domain_fields.inc
    src/resident/domains/director/world_codec/catalog_subjects_intents.inc
    src/resident/domains/director/world_codec/checkpoint_read_director.inc
    src/resident/domains/director/world_codec/checkpoint_write_original.inc
    src/resident/domains/director/world_codec/seed_original.inc
    src/resident/domains/director/world_codec/selection_registry.inc
)
