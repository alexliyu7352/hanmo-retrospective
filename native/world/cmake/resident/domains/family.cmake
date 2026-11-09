# 家庭与生命阶段：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/family/collateral_recognition_source.cpp。
set(HANMO_RESIDENT_SOURCES_family_collateral_recognition_source_cpp
    src/resident/domains/family/collateral_recognition_source.cpp
    src/resident/domains/family/collateral_recognition_receipts.cpp
    src/resident/domains/family/collateral_control_catalog.cpp
    src/resident/domains/family/collateral_recognition_codec.cpp
    src/resident/domains/family/collateral_legacy_basis.cpp
    src/resident/domains/family/collateral_control_sources.cpp
    src/resident/domains/family/collateral_control_codec.cpp
    src/resident/domains/family/collateral_control_commands.cpp
    src/resident/domains/family/collateral_control_receipts.cpp
    src/resident/domains/family/collateral_family_origin.cpp
    src/resident/domains/family/collateral_control_state.cpp
    src/resident/domains/family/collateral_family_origin_codec.cpp
    src/resident/domains/family/collateral_control_payloads.cpp
    src/resident/domains/family/collateral_family_source.cpp
    src/resident/domains/family/collateral_control_views.cpp
    src/resident/domains/family/collateral_control_world.cpp
)

# 生产输入连续登记段：src/resident/domains/family/biological_control_catalog.cpp。
set(HANMO_RESIDENT_SOURCES_family_biological_control_catalog_cpp
    src/resident/domains/family/biological_control_catalog.cpp
    src/resident/domains/family/biological_control_codec.cpp
    src/resident/domains/family/biological_control_commands.cpp
    src/resident/domains/family/biological_control_origin.cpp
    src/resident/domains/family/biological_control_origin_codec.cpp
    src/resident/domains/family/biological_control_receipts.cpp
    src/resident/domains/family/biological_control_sources.cpp
    src/resident/domains/family/biological_control_views.cpp
    src/resident/domains/family/biological_legacy_basis.cpp
    src/resident/domains/family/biological_control_world.cpp
    src/resident/domains/family/apprenticeship_legacy_basis.cpp
    src/resident/domains/family/disciple_control_sources.cpp
    src/resident/domains/family/disciple_control_receipts.cpp
    src/resident/domains/family/disciple_control_codec.cpp
    src/resident/domains/family/disciple_control_world.cpp
)

# 生产输入连续登记段：src/resident/domains/family/apprenticeship_source.cpp。
set(HANMO_RESIDENT_SOURCES_family_apprenticeship_source_cpp
    src/resident/domains/family/apprenticeship_source.cpp
    src/resident/domains/family/apprenticeship_source_codec.cpp
    src/resident/domains/family/family_apprenticeship.cpp
    src/resident/domains/family/family_apprenticeship_codec.cpp
    src/resident/domains/family/apprenticeship_receipts.cpp
    src/resident/domains/family/apprenticeship_commands.cpp
    src/resident/domains/family/apprenticeship_courses.cpp
    src/resident/domains/family/apprenticeship_views.cpp
    src/resident/domains/family/apprenticeship_world.cpp
)

# 生产输入连续登记段：src/resident/domains/family/child_care_origin.cpp。
set(HANMO_RESIDENT_SOURCES_family_child_care_origin_cpp
    src/resident/domains/family/child_care_origin.cpp
    src/resident/domains/family/child_care_relief_action.cpp
    src/resident/domains/family/child_care_relief_codec.cpp
    src/resident/domains/family/child_care_relief_commands.cpp
    src/resident/domains/family/child_care_relief_completion.cpp
    src/resident/domains/family/child_care_relief_exclusion.cpp
    src/resident/domains/family/child_care_relief_resources.cpp
    src/resident/domains/family/child_care_relief_responsibilities.cpp
    src/resident/domains/family/child_care_relief_rules.cpp
    src/resident/domains/family/child_care_relief_source.cpp
    src/resident/domains/family/child_care_relief_source_codec.cpp
    src/resident/domains/family/child_care_relief_validation.cpp
    src/resident/domains/family/child_care_relief_views.cpp
    src/resident/domains/family/child_care_relief_world.cpp
    src/resident/domains/family/actor_age_birth_read.cpp
    src/resident/domains/family/birth_arrangement_source.cpp
    src/resident/domains/family/birth_arrangement_source_codec.cpp
    src/resident/domains/family/birth_care_resources.cpp
    src/resident/domains/family/birth_endowment.cpp
    src/resident/domains/family/birth_endowment_codec.cpp
    src/resident/domains/family/family_birth_arrangements.cpp
    src/resident/domains/family/family_birth_codec.cpp
    src/resident/domains/family/family_birth_commands.cpp
    src/resident/domains/family/family_birth_creation.cpp
    src/resident/domains/family/family_birth_identity.cpp
    src/resident/domains/family/family_birth_kinship.cpp
    src/resident/domains/family/family_birth_lifecycle.cpp
    src/resident/domains/family/family_birth_lineage.cpp
    src/resident/domains/family/family_birth_responsibilities.cpp
    src/resident/domains/family/family_birth_views.cpp
    src/resident/domains/family/innate_study_source.cpp
    src/resident/domains/family/innate_study_source_codec.cpp
    src/resident/domains/family/innate_study_math.cpp
    src/resident/domains/family/innate_study_profile.cpp
    src/resident/domains/family/family_birth_world.cpp
    src/resident/domains/family/family_child_care.cpp
    src/resident/domains/family/family_child_care_completion.cpp
    src/resident/domains/family/family_child_maturation.cpp
    src/resident/domains/family/family_lineage.cpp
    src/resident/domains/family/family_lineage_codec.cpp
    src/resident/domains/family/marriage_source.cpp
    src/resident/domains/family/marriage_source_codec.cpp
    src/resident/domains/family/family_marriage.cpp
    src/resident/domains/family/family_marriage_codec.cpp
    src/resident/domains/family/family_marriage_commands.cpp
    src/resident/domains/family/family_marriage_views.cpp
    src/resident/domains/family/bequest_disclosure_source.cpp
    src/resident/domains/family/bequest_disclosure_source_codec.cpp
    src/resident/domains/family/family_inheritance.cpp
    src/resident/domains/family/family_inheritance_codec.cpp
    src/resident/domains/family/family_inheritance_settlement.cpp
    src/resident/domains/family/inheritance_personal_assets.cpp
    src/resident/domains/family/family_inheritance_commands.cpp
    src/resident/domains/family/family_inheritance_views.cpp
    src/resident/domains/family/family_inheritance_reads.cpp
)

# 生产输入连续登记段：src/resident/domains/family/born_actor_source.cpp。
set(HANMO_RESIDENT_SOURCES_family_born_actor_source_cpp
    src/resident/domains/family/born_actor_source.cpp
)

# 生产输入连续登记段：src/resident/domains/family/family_bequest_source.cpp。
set(HANMO_RESIDENT_SOURCES_family_family_bequest_source_cpp

    src/resident/domains/family/family_bequest_source.cpp
    src/resident/domains/family/family_bequest.cpp
    src/resident/domains/family/family_bequest_codec.cpp
    src/resident/domains/family/family_bequest_commands.cpp
    src/resident/domains/family/family_bequest_views.cpp
    src/resident/domains/family/family_legacy_receipt.cpp
    src/resident/domains/family/family_legacy.cpp
    src/resident/domains/family/family_legacy_codec.cpp
    src/resident/domains/family/family_legacy_source.cpp
    src/resident/domains/family/family_legacy_commands.cpp
    src/resident/domains/family/family_legacy_views.cpp
    src/resident/domains/family/legacy_control_consent.cpp
    src/resident/domains/family/legacy_control_consent_codec.cpp
    src/resident/domains/family/family_death.cpp
    src/resident/domains/family/family_death_codec.cpp
    src/resident/domains/family/family_death_views.cpp
)

# 生产输入连续登记段：src/resident/domains/family/family_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_family_family_checkpoint_cpp
    src/resident/domains/family/family_checkpoint.cpp
    src/resident/domains/family/family_codec.cpp
    src/resident/domains/family/family_checkpoint_versioned.cpp
    src/resident/domains/family/voluntary_path_catalog.cpp
    src/resident/domains/family/voluntary_path_codec.cpp
    src/resident/domains/family/voluntary_path_commands.cpp
    src/resident/domains/family/voluntary_path_original_commands.cpp
    src/resident/domains/family/voluntary_path_payloads.cpp
    src/resident/domains/family/voluntary_path_receipts.cpp
    src/resident/domains/family/voluntary_path_source.cpp
    src/resident/domains/family/voluntary_path_source_codec.cpp
    src/resident/domains/family/voluntary_path_state.cpp
    src/resident/domains/family/voluntary_path_views.cpp
    src/resident/domains/family/voluntary_relation_source.cpp
    src/resident/domains/family/voluntary_path_world.cpp
    src/resident/domains/family/voluntary_path_control.cpp
    src/resident/domains/family/succession_personal_assets.cpp
    src/resident/domains/family/succession_personal_assets_codec.cpp
    src/resident/domains/family/succession_consent_source.cpp
    src/resident/domains/family/succession_consent_codec.cpp
    src/resident/domains/family/voluntary_succession.cpp
    src/resident/domains/family/voluntary_succession_codec.cpp
    src/resident/domains/family/voluntary_succession_views.cpp
    src/resident/domains/family/adult_adoption.cpp
    src/resident/domains/family/adult_adoption_source.cpp
    src/resident/domains/family/adult_adoption_codec.cpp
    src/resident/domains/family/adult_adoption_views.cpp
    src/resident/domains/family/family_rules.cpp
    src/resident/domains/family/household_control.cpp
    src/resident/domains/family/household_control_codec.cpp
    src/resident/domains/family/character_creation.cpp
    src/resident/domains/family/character_creation_source.cpp
    src/resident/domains/family/character_creation_codec.cpp
    src/resident/domains/family/family_views.cpp
)

# 生产输入连续登记段：src/resident/domains/family/historical_player.cpp。
set(HANMO_RESIDENT_SOURCES_family_historical_player_cpp
    src/resident/domains/family/historical_player.cpp
)

# 生产输入连续登记段：src/resident/domains/family/death_notice_military_read.cpp。
set(HANMO_RESIDENT_SOURCES_family_death_notice_military_read_cpp
    src/resident/domains/family/death_notice_military_read.cpp
    src/resident/domains/family/death_notice_source_codec.cpp
    src/resident/domains/family/death_notice_source.cpp
)

# 生产输入连续登记段：src/resident/domains/family/family_life_exact.cpp。
set(HANMO_RESIDENT_SOURCES_family_family_life_exact_cpp
    src/resident/domains/family/family_life_exact.cpp
    src/resident/domains/family/family_life_exact_codec.cpp
    src/resident/domains/family/family_origin_v2.cpp
    src/resident/domains/family/life_age.cpp
    src/resident/domains/family/life_age_codec.cpp
    src/resident/domains/family/life_age_exact_phase.cpp
    src/resident/domains/family/profile_control_eligibility.cpp
)

# 生产输入连续登记段：src/resident/domains/family/care_handover_start.cpp。
set(HANMO_RESIDENT_SOURCES_family_care_handover_start_cpp
    src/resident/domains/family/care_handover_start.cpp
    src/resident/domains/family/care_handover_completion.cpp
)

# 生产输入连续登记段：src/resident/domains/family/birth_due_execution.cpp。
set(HANMO_RESIDENT_SOURCES_family_birth_due_execution_cpp
    src/resident/domains/family/birth_due_execution.cpp
    src/resident/domains/family/birth_due_execution_codec.cpp
    src/resident/domains/family/born_character_creation.cpp
    src/resident/domains/family/born_character_source.cpp
    src/resident/domains/family/care_arrangement.cpp
    src/resident/domains/family/care_arrangement_codec.cpp
    src/resident/domains/family/care_arrangement_retention.cpp
    src/resident/domains/family/care_coverage.cpp
    src/resident/domains/family/care_coverage_codec.cpp
    src/resident/domains/family/care_coverage_placement.cpp
    src/resident/domains/family/care_coverage_retention.cpp
    src/resident/domains/family/care_facility_use.cpp
    src/resident/domains/family/care_facility_use_codec.cpp
    src/resident/domains/family/care_facility_use_retention.cpp
    src/resident/domains/family/care_family_activity.cpp
    src/resident/domains/family/care_family_activity_codec.cpp
    src/resident/domains/family/care_family_activity_read.cpp
    src/resident/domains/family/care_family_activity_retention.cpp
    src/resident/domains/family/care_growth_participation.cpp
    src/resident/domains/family/care_handover_command_source.cpp
    src/resident/domains/family/care_handover_execution.cpp
    src/resident/domains/family/care_handover_execution_codec.cpp
    src/resident/domains/family/care_handover_period.cpp
    src/resident/domains/family/care_handover_retention.cpp
    src/resident/domains/family/care_handover_settlement.cpp
    src/resident/domains/family/care_handover_time.cpp
    src/resident/domains/family/care_handover_worker_read.cpp
    src/resident/domains/family/care_life_condition.cpp
    src/resident/domains/family/care_participant_capacity.cpp
    src/resident/domains/family/care_period.cpp
    src/resident/domains/family/care_period_codec.cpp
    src/resident/domains/family/care_period_original_action.cpp
    src/resident/domains/family/care_period_retention.cpp
    src/resident/domains/family/care_personal_stop.cpp
    src/resident/domains/family/care_requirement_author.cpp
    src/resident/domains/family/care_requirement_contract.cpp
    src/resident/domains/family/care_requirement_record_codec.cpp
    src/resident/domains/family/care_requirement_retention.cpp
    src/resident/domains/family/care_requirement_rules.cpp
    src/resident/domains/family/care_separation.cpp
    src/resident/domains/family/control_continuation.cpp
    src/resident/domains/family/control_continuation_codec.cpp
    src/resident/domains/family/control_retirement_cancel.cpp
    src/resident/domains/family/dependent_provision_profile.cpp
    src/resident/domains/family/family_continuation_people4.cpp
    src/resident/domains/family/family_group_ref.cpp
    src/resident/domains/family/family_inheritance_sources2.cpp
    src/resident/domains/family/family_life_history4.cpp
    src/resident/domains/family/life_age_basis.cpp
    src/resident/domains/family/native_continuation_sources.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/collateral_recognition_source.cpp。
set(HANMO_RESIDENT_IDENTITY_family_collateral_recognition_source_cpp
    src/resident/domains/family/collateral_recognition_source.cpp
    src/resident/domains/family/collateral_recognition_receipts.cpp
    src/resident/domains/family/collateral_control_catalog.cpp
    src/resident/domains/family/collateral_recognition_codec.cpp
    src/resident/domains/family/collateral_legacy_basis.cpp
    src/resident/domains/family/collateral_legacy_basis.hpp
    src/resident/domains/family/collateral_control_sources.cpp
    src/resident/domains/family/collateral_control_payloads.hpp
    src/resident/domains/family/collateral_control_codec.cpp
    src/resident/domains/family/collateral_control_commands.cpp
    src/resident/domains/family/collateral_control_receipts.hpp
    src/resident/domains/family/collateral_control_commands.hpp
    src/resident/domains/family/collateral_control_receipts.cpp
    src/resident/domains/family/collateral_family_origin.cpp
    src/resident/domains/family/collateral_control_sources.hpp
    src/resident/domains/family/collateral_recognition_receipts.hpp
    src/resident/domains/family/collateral_control_catalog.hpp
    src/resident/domains/family/collateral_control_state.cpp
    src/resident/domains/family/collateral_family_origin_codec.cpp
    src/resident/domains/family/collateral_control_payloads.cpp
    src/resident/domains/family/collateral_family_source.cpp
    src/resident/domains/family/collateral_control_state.hpp
    src/resident/domains/family/collateral_recognition_source.hpp
    src/resident/domains/family/collateral_control_views.cpp
    src/resident/domains/family/collateral_family_origin.hpp
    src/resident/domains/family/collateral_family_source.hpp
    src/resident/domains/family/collateral_control_world.hpp
    src/resident/domains/family/collateral_control_world.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/biological_control_catalog.cpp。
set(HANMO_RESIDENT_IDENTITY_family_biological_control_catalog_cpp
    src/resident/domains/family/biological_control_catalog.cpp
    src/resident/domains/family/biological_control_codec.cpp
    src/resident/domains/family/biological_control_commands.cpp
    src/resident/domains/family/biological_control_origin.cpp
    src/resident/domains/family/biological_control_origin_codec.cpp
    src/resident/domains/family/biological_control_receipts.cpp
    src/resident/domains/family/biological_control_sources.cpp
    src/resident/domains/family/biological_control_views.cpp
    src/resident/domains/family/biological_legacy_basis.cpp
    src/resident/domains/family/biological_control_world.cpp
    src/resident/domains/family/biological_control_catalog.hpp
    src/resident/domains/family/biological_control_commands.hpp
    src/resident/domains/family/biological_control_origin.hpp
    src/resident/domains/family/biological_control_receipts.hpp
    src/resident/domains/family/biological_control_sources.hpp
    src/resident/domains/family/biological_legacy_basis.hpp
    src/resident/domains/family/biological_control_world.hpp
    src/resident/domains/family/apprenticeship_legacy_basis.cpp
    src/resident/domains/family/disciple_control_sources.cpp
    src/resident/domains/family/disciple_control_receipts.cpp
    src/resident/domains/family/disciple_control_codec.cpp
    src/resident/domains/family/disciple_control_world.cpp
    src/resident/domains/family/apprenticeship_legacy_basis.hpp
    src/resident/domains/family/disciple_control_sources.hpp
    src/resident/domains/family/disciple_control_receipts.hpp
    src/resident/domains/family/disciple_control_world.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/apprenticeship_source.cpp。
set(HANMO_RESIDENT_IDENTITY_family_apprenticeship_source_cpp
    src/resident/domains/family/apprenticeship_source.cpp
    src/resident/domains/family/apprenticeship_source_codec.cpp
    src/resident/domains/family/family_apprenticeship.cpp
    src/resident/domains/family/family_apprenticeship_codec.cpp
    src/resident/domains/family/apprenticeship_receipts.cpp
    src/resident/domains/family/apprenticeship_commands.cpp
    src/resident/domains/family/apprenticeship_courses.cpp
    src/resident/domains/family/apprenticeship_views.cpp
    src/resident/domains/family/apprenticeship_world.cpp
    src/resident/domains/family/apprenticeship_source.hpp
    src/resident/domains/family/family_apprenticeship.hpp
    src/resident/domains/family/apprenticeship_receipts.hpp
    src/resident/domains/family/apprenticeship_commands.hpp
    src/resident/domains/family/apprenticeship_courses.hpp
    src/resident/domains/family/apprenticeship_views.hpp
    src/resident/domains/family/apprenticeship_world.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/child_care_origin.cpp。
set(HANMO_RESIDENT_IDENTITY_family_child_care_origin_cpp
    src/resident/domains/family/child_care_origin.cpp
    src/resident/domains/family/child_care_origin.hpp
    src/resident/domains/family/child_care_relief.hpp
    src/resident/domains/family/child_care_relief_action.cpp
    src/resident/domains/family/child_care_relief_action.hpp
    src/resident/domains/family/child_care_relief_codec.cpp
    src/resident/domains/family/child_care_relief_commands.cpp
    src/resident/domains/family/child_care_relief_commands.hpp
    src/resident/domains/family/child_care_relief_completion.cpp
    src/resident/domains/family/child_care_relief_completion.hpp
    src/resident/domains/family/child_care_relief_cache.hpp
    src/resident/domains/family/child_care_relief_exclusion.cpp
    src/resident/domains/family/child_care_relief_exclusion.hpp
    src/resident/domains/family/child_care_relief_resources.cpp
    src/resident/domains/family/child_care_relief_resources.hpp
    src/resident/domains/family/child_care_relief_responsibilities.cpp
    src/resident/domains/family/child_care_relief_responsibilities.hpp
    src/resident/domains/family/child_care_relief_rules.cpp
    src/resident/domains/family/child_care_relief_source.cpp
    src/resident/domains/family/child_care_relief_source.hpp
    src/resident/domains/family/child_care_relief_source_codec.cpp
    src/resident/domains/family/child_care_relief_transaction.hpp
    src/resident/domains/family/child_care_relief_validation.cpp
    src/resident/domains/family/child_care_relief_views.cpp
    src/resident/domains/family/child_care_relief_world.cpp
    src/resident/domains/family/actor_age_birth_read.cpp
    src/resident/domains/family/child_care_relief_world.hpp
    src/resident/domains/family/birth_arrangement_source.cpp
    src/resident/domains/family/birth_arrangement_source.hpp
    src/resident/domains/family/birth_arrangement_source_codec.cpp
    src/resident/domains/family/birth_care_resources.cpp
    src/resident/domains/family/birth_care_resources.hpp
    src/resident/domains/family/birth_endowment.cpp
    src/resident/domains/family/birth_endowment.hpp
    src/resident/domains/family/birth_endowment_codec.cpp
    src/resident/domains/family/family_birth.hpp
    src/resident/domains/family/family_birth_arrangements.cpp
    src/resident/domains/family/family_birth_codec.cpp
    src/resident/domains/family/family_birth_commands.cpp
    src/resident/domains/family/family_birth_commands.hpp
    src/resident/domains/family/family_birth_creation.cpp
    src/resident/domains/family/family_birth_creation.hpp
    src/resident/domains/family/family_birth_identity.cpp
    src/resident/domains/family/family_birth_identity.hpp
    src/resident/domains/family/family_birth_kinship.cpp
    src/resident/domains/family/family_birth_kinship.hpp
    src/resident/domains/family/family_birth_lifecycle.cpp
    src/resident/domains/family/family_birth_lifecycle.hpp
    src/resident/domains/family/family_birth_lineage.cpp
    src/resident/domains/family/family_birth_lineage.hpp
    src/resident/domains/family/family_birth_responsibilities.cpp
    src/resident/domains/family/family_birth_responsibilities.hpp
    src/resident/domains/family/family_birth_transaction.hpp
    src/resident/domains/family/family_birth_views.cpp
    src/resident/domains/family/innate_study_source.cpp
    src/resident/domains/family/innate_study_source_codec.cpp
    src/resident/domains/family/innate_study_math.cpp
    src/resident/domains/family/innate_study_profile.cpp
    src/resident/domains/family/family_birth_world.cpp
    src/resident/domains/family/innate_study_model.hpp
    src/resident/domains/family/innate_study_source.hpp
    src/resident/domains/family/innate_study_profile.hpp
    src/resident/domains/family/family_birth_world.hpp
    src/resident/domains/family/family_child_care.cpp
    src/resident/domains/family/family_child_care.hpp
    src/resident/domains/family/family_child_care_action_window.hpp
    src/resident/domains/family/family_child_care_completion.cpp
    src/resident/domains/family/family_child_care_completion.hpp
    src/resident/domains/family/family_child_maturation.cpp
    src/resident/domains/family/family_child_maturation.hpp
    src/resident/domains/family/family_lineage.cpp
    src/resident/domains/family/family_lineage.hpp
    src/resident/domains/family/family_lineage_codec.cpp
    src/resident/domains/family/marriage_source.cpp
    src/resident/domains/family/marriage_source_codec.cpp
    src/resident/domains/family/family_marriage.cpp
    src/resident/domains/family/family_marriage_codec.cpp
    src/resident/domains/family/family_marriage_commands.cpp
    src/resident/domains/family/family_marriage_views.cpp
    src/resident/domains/family/marriage_source.hpp
    src/resident/domains/family/family_marriage.hpp
    src/resident/domains/family/family_marriage_commands.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/bequest_disclosure_source.cpp。
set(HANMO_RESIDENT_IDENTITY_family_bequest_disclosure_source_cpp
    src/resident/domains/family/bequest_disclosure_source.cpp
    src/resident/domains/family/bequest_disclosure_source_codec.cpp
    src/resident/domains/family/family_inheritance.cpp
    src/resident/domains/family/family_inheritance_codec.cpp
    src/resident/domains/family/family_inheritance_settlement.cpp
    src/resident/domains/family/inheritance_personal_assets.cpp
    src/resident/domains/family/family_inheritance_commands.cpp
    src/resident/domains/family/family_inheritance_views.cpp
    src/resident/domains/family/family_inheritance_reads.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/born_actor_source.cpp。
set(HANMO_RESIDENT_IDENTITY_family_born_actor_source_cpp
    src/resident/domains/family/born_actor_source.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/bequest_disclosure_source.hpp。
set(HANMO_RESIDENT_IDENTITY_family_bequest_disclosure_source_hpp
    src/resident/domains/family/bequest_disclosure_source.hpp
    src/resident/domains/family/family_inheritance.hpp
    src/resident/domains/family/inheritance_personal_assets.hpp
    src/resident/domains/family/family_inheritance_commands.hpp
    src/resident/domains/family/family_inheritance_reads.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/born_actor_source.hpp。
set(HANMO_RESIDENT_IDENTITY_family_born_actor_source_hpp
    src/resident/domains/family/born_actor_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/death_notice_military_read.hpp。
set(HANMO_RESIDENT_IDENTITY_family_death_notice_military_read_hpp
    src/resident/domains/family/death_notice_military_read.hpp
    src/resident/domains/family/death_notice_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/family_bequest_source.cpp。
set(HANMO_RESIDENT_IDENTITY_family_family_bequest_source_cpp
    src/resident/domains/family/family_bequest_source.cpp
    src/resident/domains/family/family_bequest.cpp
    src/resident/domains/family/family_bequest_codec.cpp
    src/resident/domains/family/family_bequest_commands.cpp
    src/resident/domains/family/family_bequest_views.cpp
    src/resident/domains/family/family_bequest_source.hpp
    src/resident/domains/family/family_bequest.hpp
    src/resident/domains/family/family_bequest_commands.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/family_legacy_receipt.cpp。
set(HANMO_RESIDENT_IDENTITY_family_family_legacy_receipt_cpp
    src/resident/domains/family/family_legacy_receipt.cpp
    src/resident/domains/family/family_legacy.cpp
    src/resident/domains/family/family_legacy_codec.cpp
    src/resident/domains/family/family_legacy_source.cpp
    src/resident/domains/family/family_legacy_commands.cpp
    src/resident/domains/family/family_legacy_views.cpp
    src/resident/domains/family/legacy_control_consent.cpp
    src/resident/domains/family/legacy_control_consent_codec.cpp
    src/resident/domains/family/family_death.cpp
    src/resident/domains/family/family_death_codec.cpp
    src/resident/domains/family/family_death_views.cpp
    src/resident/domains/family/family_legacy_receipt.hpp
    src/resident/domains/family/family_legacy.hpp
    src/resident/domains/family/family_legacy_source.hpp
    src/resident/domains/family/family_legacy_commands.hpp
    src/resident/domains/family/legacy_control_consent.hpp
    src/resident/domains/family/family_death.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/actor_age_read.hpp。
set(HANMO_RESIDENT_IDENTITY_family_actor_age_read_hpp
    src/resident/domains/family/actor_age_read.hpp
    src/resident/domains/family/actor_age_birth_read.hpp
    src/resident/domains/family/birth_care_cached_revision.hpp
    src/resident/domains/family/family_care_completion_shape.hpp
    src/resident/domains/family/actor_presence_read.hpp
    src/resident/domains/family/actor_death_read.hpp
    src/resident/domains/family/actor_life_eligibility.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/family_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_family_family_checkpoint_cpp
    src/resident/domains/family/family_checkpoint.cpp
    src/resident/domains/family/family_codec.cpp
    src/resident/domains/family/family_checkpoint_versioned.cpp
    src/resident/domains/family/voluntary_path_catalog.cpp
    src/resident/domains/family/voluntary_path_codec.cpp
    src/resident/domains/family/voluntary_path_commands.cpp
    src/resident/domains/family/voluntary_path_original_commands.cpp
    src/resident/domains/family/voluntary_path_payloads.cpp
    src/resident/domains/family/voluntary_path_receipts.cpp
    src/resident/domains/family/voluntary_path_source.cpp
    src/resident/domains/family/voluntary_path_source_codec.cpp
    src/resident/domains/family/voluntary_path_state.cpp
    src/resident/domains/family/voluntary_path_views.cpp
    src/resident/domains/family/voluntary_relation_source.cpp
    src/resident/domains/family/voluntary_path_world.cpp
    src/resident/domains/family/voluntary_path_control.cpp
    src/resident/domains/family/succession_personal_assets.cpp
    src/resident/domains/family/succession_personal_assets_codec.cpp
    src/resident/domains/family/succession_consent_source.cpp
    src/resident/domains/family/succession_consent_codec.cpp
    src/resident/domains/family/voluntary_succession.cpp
    src/resident/domains/family/voluntary_succession_codec.cpp
    src/resident/domains/family/voluntary_succession_views.cpp
    src/resident/domains/family/adult_adoption.cpp
    src/resident/domains/family/adult_adoption_source.cpp
    src/resident/domains/family/adult_adoption_codec.cpp
    src/resident/domains/family/adult_adoption_views.cpp
    src/resident/domains/family/family_codec.hpp
    src/resident/domains/family/family_model.hpp
    src/resident/domains/family/family_checkpoint_versioned.hpp
    src/resident/domains/family/family_codec_version.hpp
    src/resident/domains/family/voluntary_family_readiness.hpp
    src/resident/domains/family/voluntary_path_catalog.hpp
    src/resident/domains/family/voluntary_path_commands.hpp
    src/resident/domains/family/voluntary_path_original_commands.hpp
    src/resident/domains/family/voluntary_path_payloads.hpp
    src/resident/domains/family/voluntary_path_receipts.hpp
    src/resident/domains/family/voluntary_path_source.hpp
    src/resident/domains/family/voluntary_path_state.hpp
    src/resident/domains/family/voluntary_path_views.hpp
    src/resident/domains/family/voluntary_relation_source.hpp
    src/resident/domains/family/voluntary_succession_candidate.hpp
    src/resident/domains/family/voluntary_path_world.hpp
    src/resident/domains/family/voluntary_path_control.hpp
    src/resident/domains/family/succession_personal_assets.hpp
    src/resident/domains/family/succession_consent_source.hpp
    src/resident/domains/family/voluntary_succession.hpp
    src/resident/domains/family/voluntary_succession_model.hpp
    src/resident/domains/family/adult_adoption.hpp
    src/resident/domains/family/adult_adoption_source.hpp
    src/resident/domains/family/household_control.hpp
    src/resident/domains/family/character_creation.hpp
    src/resident/domains/family/character_creation_source.hpp
    src/resident/domains/family/family_rules.cpp
    src/resident/domains/family/household_control.cpp
    src/resident/domains/family/household_control_codec.cpp
    src/resident/domains/family/character_creation.cpp
    src/resident/domains/family/character_creation_source.cpp
    src/resident/domains/family/character_creation_codec.cpp
    src/resident/domains/family/family_views.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/historical_player.cpp。
set(HANMO_RESIDENT_IDENTITY_family_historical_player_cpp
    src/resident/domains/family/historical_player.cpp
    src/resident/domains/family/historical_player.hpp
)

# 生产身份输入连续登记段：src/resident/domains/family/death_notice_military_read.cpp。
set(HANMO_RESIDENT_IDENTITY_family_death_notice_military_read_cpp
    src/resident/domains/family/death_notice_military_read.cpp
    src/resident/domains/family/death_notice_source_codec.cpp
    src/resident/domains/family/death_notice_source.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/family_life_exact.cpp。
set(HANMO_RESIDENT_IDENTITY_family_family_life_exact_cpp
    src/resident/domains/family/family_life_exact.cpp
    src/resident/domains/family/family_life_exact_codec.cpp
    src/resident/domains/family/family_origin_v2.cpp
    src/resident/domains/family/life_age.cpp
    src/resident/domains/family/life_age_codec.cpp
    src/resident/domains/family/life_age_exact_phase.cpp
    src/resident/domains/family/profile_control_eligibility.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/care_handover_start.cpp。
set(HANMO_RESIDENT_IDENTITY_family_care_handover_start_cpp
    src/resident/domains/family/care_handover_start.cpp
    src/resident/domains/family/care_handover_completion.cpp
)

# 生产身份输入连续登记段：src/resident/domains/family/birth_due_execution.cpp。
set(HANMO_RESIDENT_IDENTITY_family_birth_due_execution_cpp
    src/resident/domains/family/birth_due_execution.cpp
    src/resident/domains/family/birth_due_execution_codec.cpp
    src/resident/domains/family/born_character_creation.cpp
    src/resident/domains/family/born_character_source.cpp
    src/resident/domains/family/care_arrangement.cpp
    src/resident/domains/family/care_arrangement_codec.cpp
    src/resident/domains/family/care_arrangement_retention.cpp
    src/resident/domains/family/care_coverage.cpp
    src/resident/domains/family/care_coverage_codec.cpp
    src/resident/domains/family/care_coverage_placement.cpp
    src/resident/domains/family/care_coverage_retention.cpp
    src/resident/domains/family/care_facility_use.cpp
    src/resident/domains/family/care_facility_use_codec.cpp
    src/resident/domains/family/care_facility_use_retention.cpp
    src/resident/domains/family/care_family_activity.cpp
    src/resident/domains/family/care_family_activity_codec.cpp
    src/resident/domains/family/care_family_activity_read.cpp
    src/resident/domains/family/care_family_activity_retention.cpp
    src/resident/domains/family/care_growth_participation.cpp
    src/resident/domains/family/care_handover_command_source.cpp
    src/resident/domains/family/care_handover_execution.cpp
    src/resident/domains/family/care_handover_execution_codec.cpp
    src/resident/domains/family/care_handover_period.cpp
    src/resident/domains/family/care_handover_retention.cpp
    src/resident/domains/family/care_handover_settlement.cpp
    src/resident/domains/family/care_handover_time.cpp
    src/resident/domains/family/care_handover_worker_read.cpp
    src/resident/domains/family/care_life_condition.cpp
    src/resident/domains/family/care_participant_capacity.cpp
    src/resident/domains/family/care_period.cpp
    src/resident/domains/family/care_period_codec.cpp
    src/resident/domains/family/care_period_original_action.cpp
    src/resident/domains/family/care_period_retention.cpp
    src/resident/domains/family/care_personal_stop.cpp
    src/resident/domains/family/care_requirement_author.cpp
    src/resident/domains/family/care_requirement_contract.cpp
    src/resident/domains/family/care_requirement_record_codec.cpp
    src/resident/domains/family/care_requirement_retention.cpp
    src/resident/domains/family/care_requirement_rules.cpp
    src/resident/domains/family/care_separation.cpp
    src/resident/domains/family/control_continuation.cpp
    src/resident/domains/family/control_continuation_codec.cpp
    src/resident/domains/family/control_retirement_cancel.cpp
    src/resident/domains/family/dependent_provision_profile.cpp
    src/resident/domains/family/family_continuation_people4.cpp
    src/resident/domains/family/family_group_ref.cpp
    src/resident/domains/family/family_inheritance_sources2.cpp
    src/resident/domains/family/family_life_history4.cpp
    src/resident/domains/family/life_age_basis.cpp
    src/resident/domains/family/native_continuation_sources.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/family/source_owned_fields/birth_care.inc
    src/resident/domains/family/source_owned_fields/relationships_identity.inc
    src/resident/domains/family/source_owned_fields/succession_control.inc
    src/resident/domains/family/world_codec/apprenticeship_fact_projection.inc
    src/resident/domains/family/world_codec/catalog_apprenticeship.inc
    src/resident/domains/family/world_codec/catalog_biological_collateral.inc
    src/resident/domains/family/world_codec/catalog_care_legacy.inc
    src/resident/domains/family/world_codec/catalog_inheritance_birth.inc
    src/resident/domains/family/world_codec/catalog_life.inc
    src/resident/domains/family/world_codec/catalog_voluntary_paths.inc
    src/resident/domains/family/world_codec/checkpoint_read_control_continuation.inc
    src/resident/domains/family/world_codec/checkpoint_read_family.inc
    src/resident/domains/family/world_codec/checkpoint_read_family_apprenticeship.inc
    src/resident/domains/family/world_codec/checkpoint_read_family_biological_legacy.inc
    src/resident/domains/family/world_codec/checkpoint_read_family_collateral_legacy.inc
    src/resident/domains/family/world_codec/checkpoint_read_family_disciple_legacy.inc
    src/resident/domains/family/world_codec/checkpoint_read_family_life.inc
    src/resident/domains/family/world_codec/seed_age_control.inc
    src/resident/domains/family/world_codec/seed_legacy_paths.inc
    src/resident/domains/family/world_codec/seed_lifecycle.inc
    src/resident/domains/family/world_codec/seed_validate_apprenticeship.inc
    src/resident/domains/family/world_codec/seed_validate_collateral.inc
    src/resident/domains/family/world_codec/seed_voluntary_paths.inc
    src/resident/domains/family/world_codec/selection_birth_succession.inc
    src/resident/domains/family/world_codec/selection_creation.inc
    src/resident/domains/family/world_codec/selection_lifecycle.inc
)

# 已定出生/照护来源叶及强型值；不注册完整阶段或准入能力。
list(APPEND HANMO_RESIDENT_SOURCES_family_birth_due_execution_cpp
    src/resident/domains/family/birth_due_care_source.cpp
    src/resident/domains/family/birth_due_original_born.cpp
    src/resident/domains/family/birth_due_prediction.cpp
    src/resident/domains/family/care_body_scope.cpp
    src/resident/domains/family/care_facility_named_body.cpp
    src/resident/domains/family/care_facility_occupancy.cpp
    src/resident/domains/family/care_facility_placement_source.cpp
    src/resident/domains/family/care_clearance_history_values.cpp
    src/resident/domains/family/care_maturity_record.cpp
    src/resident/domains/family/child_maturation_exact.cpp
)

# 同一生产源码与原值头进入真实核心身份清单，不以测试文件替代生产身份。
list(APPEND HANMO_RESIDENT_IDENTITY_family_birth_due_execution_cpp
    src/resident/domains/family/birth_due_care_source.cpp
    src/resident/domains/family/birth_due_care_source.hpp
    src/resident/domains/family/birth_due_original_born.cpp
    src/resident/domains/family/birth_due_original_born.hpp
    src/resident/domains/family/birth_due_prediction.cpp
    src/resident/domains/family/birth_due_prediction.hpp
    src/resident/domains/family/care_body_scope.cpp
    src/resident/domains/family/care_facility_named_body.cpp
    src/resident/domains/family/care_facility_occupancy.cpp
    src/resident/domains/family/care_facility_placement_source.cpp
    src/resident/domains/family/care_body_scope.hpp
    src/resident/domains/family/care_body_scope_codec_fields.hpp
    src/resident/domains/family/care_facility_named_body.hpp
    src/resident/domains/family/care_facility_named_body_codec_fields.hpp
    src/resident/domains/family/care_facility_occupancy.hpp
    src/resident/domains/family/care_facility_occupancy_codec_fields.hpp
    src/resident/domains/family/care_facility_placement_source.hpp
    src/resident/domains/family/care_clearance_history_values.cpp
    src/resident/domains/family/care_clearance_history_values.hpp
    src/resident/domains/family/care_clearance_history_values_codec_fields.hpp
    src/resident/domains/family/care_clearance_origin.hpp
    src/resident/domains/family/care_clearance_origin_codec_fields.hpp
    src/resident/domains/family/care_maturity_record.cpp
    src/resident/domains/family/care_maturity_record.hpp
    src/resident/domains/family/care_maturity_record_codec_fields.hpp
    src/resident/domains/family/care_physical_position.hpp
    src/resident/domains/family/care_physical_position_codec_fields.hpp
    src/resident/domains/family/care_root_layout.hpp
    src/resident/domains/family/care_transport_carrier.hpp
    src/resident/domains/family/care_transport_carrier_codec_fields.hpp
    src/resident/domains/family/child_maturation_exact.cpp
    src/resident/domains/family/child_maturation_exact.hpp
)

# 拆分字段片段随原翻译单元包含，同时保留生产身份闭包。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/family/source_owned_fields/care_due_values.inc
    src/resident/domains/family/source_owned_fields/care_transport_carrier.inc
)

# Family 原20值的复制/保留片段，按原登记入口包含并纳入生产身份。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)

# 原 Care 最终目录选择只接真实 prepare/拥有图，不安装完整根或教育来源。
list(APPEND HANMO_RESIDENT_SOURCES_family_birth_due_execution_cpp
    src/resident/kernel/native_care_root_selection.cpp
)
list(APPEND HANMO_RESIDENT_IDENTITY_family_birth_due_execution_cpp
    src/resident/domains/family/care_root_capability_selection.hpp
    src/resident/domains/family/care_root_selected_layout.hpp
    src/resident/kernel/native_care_root_selection.cpp
)
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/family/source_owned_fields/care_root_selection.inc
)
