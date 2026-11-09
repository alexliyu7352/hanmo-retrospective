# 玩家计划：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/player_plan/plan_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_player_plan_plan_checkpoint_cpp

    src/resident/domains/player_plan/plan_checkpoint.cpp
    src/resident/domains/player_plan/plan_codec.cpp
    src/resident/domains/player_plan/plan_binding.cpp
    src/resident/domains/player_plan/plan_contact_inquiry.cpp
    src/resident/domains/player_plan/plan_message_return.cpp
    src/resident/domains/player_plan/plan_personal_message.cpp
    src/resident/domains/player_plan/plan_personal_report.cpp
    src/resident/domains/player_plan/plan_personal_acceptance.cpp
    src/resident/domains/player_plan/plan_visit_appointment.cpp
    src/resident/domains/player_plan/plan_tactic_lesson.cpp
    src/resident/domains/player_plan/plan_execution.cpp
    src/resident/domains/player_plan/plan_lifecycle.cpp
    src/resident/domains/player_plan/plan_page.cpp
    src/resident/domains/player_plan/plan_sources.cpp
    src/resident/domains/player_plan/plan_validation.cpp
    src/resident/domains/player_plan/plan_views.cpp
)

# 生产输入连续登记段：src/resident/domains/player_plan/plan_profile.cpp。
set(HANMO_RESIDENT_SOURCES_player_plan_plan_profile_cpp
    src/resident/domains/player_plan/plan_profile.cpp
    src/resident/domains/player_plan/plan_time2.cpp
)

# 生产身份输入连续登记段：src/resident/domains/player_plan/plan_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_player_plan_plan_checkpoint_cpp
    src/resident/domains/player_plan/plan_checkpoint.cpp
    src/resident/domains/player_plan/plan_codec.cpp
    src/resident/domains/player_plan/plan_binding.cpp
    src/resident/domains/player_plan/plan_contact_inquiry.cpp
    src/resident/domains/player_plan/plan_message_return.cpp
    src/resident/domains/player_plan/plan_personal_message.cpp
    src/resident/domains/player_plan/plan_personal_report.cpp
    src/resident/domains/player_plan/plan_personal_acceptance.cpp
    src/resident/domains/player_plan/plan_visit_appointment.cpp
    src/resident/domains/player_plan/plan_tactic_lesson.cpp
    src/resident/domains/player_plan/plan_execution.cpp
    src/resident/domains/player_plan/plan_lifecycle.cpp
    src/resident/domains/player_plan/plan_page.cpp
    src/resident/domains/player_plan/plan_sources.cpp
    src/resident/domains/player_plan/plan_validation.cpp
    src/resident/domains/player_plan/plan_views.cpp
)

# 生产身份输入连续登记段：src/resident/domains/player_plan/plan_codec.hpp。
set(HANMO_RESIDENT_IDENTITY_player_plan_plan_codec_hpp
    src/resident/domains/player_plan/plan_codec.hpp
    src/resident/domains/player_plan/plan_commands.hpp
    src/resident/domains/player_plan/plan_binding.hpp
    src/resident/domains/player_plan/plan_execution.hpp
    src/resident/domains/player_plan/plan_lifecycle.hpp
    src/resident/domains/player_plan/plan_model.hpp
    src/resident/domains/player_plan/plan_page.hpp
    src/resident/domains/player_plan/plan_sources.hpp
    src/resident/domains/player_plan/plan_views.hpp
)

# 生产身份输入连续登记段：src/resident/domains/player_plan/plan_profile.cpp。
set(HANMO_RESIDENT_IDENTITY_player_plan_plan_profile_cpp
    src/resident/domains/player_plan/plan_profile.cpp
    src/resident/domains/player_plan/plan_profile.hpp
    src/resident/domains/player_plan/plan_time2.cpp
    src/resident/domains/player_plan/plan_time2.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/player_plan/source_owned_fields/domain_fields.inc
    src/resident/domains/player_plan/world_codec/checkpoint_read_player_plan.inc
    src/resident/domains/player_plan/world_codec/seed_original.inc
    src/resident/domains/player_plan/world_codec/selection_version.inc
)
