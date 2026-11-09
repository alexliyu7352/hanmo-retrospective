# 组织与成员：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/organization/institutional_obligation_source.cpp。
set(HANMO_RESIDENT_SOURCES_organization_institutional_obligation_source_cpp
    src/resident/domains/organization/institutional_obligation_source.cpp
)

# 生产输入连续登记段：src/resident/domains/organization/organization_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_organization_organization_checkpoint_cpp
    src/resident/domains/organization/organization_checkpoint.cpp
    src/resident/domains/organization/organization_codec.cpp
    src/resident/domains/organization/organization_rules.cpp
    src/resident/domains/organization/organization_link_not_implemented.cpp
    src/resident/domains/organization/named_membership_rules.cpp
    src/resident/domains/organization/named_membership_validation.cpp
    src/resident/domains/organization/membership_offer.cpp
    src/resident/domains/organization/organization_contract_authority.cpp
    src/resident/domains/organization/organization_validation.cpp
    src/resident/domains/organization/organization_views.cpp
)

# 生产输入连续登记段：src/resident/domains/organization/authority_basis.cpp。
set(HANMO_RESIDENT_SOURCES_organization_authority_basis_cpp
    src/resident/domains/organization/authority_basis.cpp
    src/resident/domains/organization/authority_basis_codec.cpp
    src/resident/domains/organization/authority_change_read.cpp
    src/resident/domains/organization/authority_registry.cpp
    src/resident/domains/organization/grant_authority.cpp
    src/resident/domains/organization/grant_terms_definition.cpp
    src/resident/domains/organization/membership_office_transition.cpp
    src/resident/domains/organization/office_authority.cpp
    src/resident/domains/organization/office_lifecycle.cpp
    src/resident/domains/organization/organization_activity_history.cpp
    src/resident/domains/organization/organization_authority_codec.cpp
    src/resident/domains/organization/organization_calendar.cpp
    src/resident/domains/organization/organization_fact_source.cpp
    src/resident/domains/organization/organization_lifecycle_contract.cpp
    src/resident/domains/organization/organization_original_runtime.cpp
    src/resident/domains/organization/organization_runtime.cpp
    src/resident/domains/organization/organization_runtime_codec.cpp
    src/resident/domains/organization/organization_authored_origin.cpp
    src/resident/domains/organization/organization_authored_lifecycle.cpp
    src/resident/domains/organization/organization_resource_validation_reads.cpp
    src/resident/domains/organization/organization_runtime_body_codec.cpp
    src/resident/domains/organization/organization_service_effects_codec.cpp
    src/resident/domains/organization/political_office_transition.cpp
    src/resident/domains/organization/public_market_organization.cpp
)

# 生产输入连续登记段：src/resident/domains/organization/organization_lifecycle_catalog.cpp。
set(HANMO_RESIDENT_SOURCES_organization_organization_lifecycle_catalog_cpp
    src/resident/domains/organization/organization_lifecycle_catalog.cpp
)

# 生产输入连续登记段：src/resident/domains/organization/organization_activity_policy.cpp。
set(HANMO_RESIDENT_SOURCES_organization_organization_activity_policy_cpp
    src/resident/domains/organization/organization_activity_policy.cpp
    src/resident/domains/organization/organization_activity_policy_codec.cpp
    src/resident/domains/organization/organization_calendar_admission.cpp
    src/resident/domains/organization/organization_population_training_source.cpp
)

# 生产身份输入连续登记段：src/resident/domains/organization/organization_validation.hpp。
set(HANMO_RESIDENT_IDENTITY_organization_organization_validation_hpp
    src/resident/domains/organization/organization_validation.hpp
    src/resident/domains/organization/organization_validation_detail.hpp
)

# 生产身份输入连续登记段：src/resident/domains/organization/institutional_obligation_source.cpp。
set(HANMO_RESIDENT_IDENTITY_organization_institutional_obligation_source_cpp
    src/resident/domains/organization/institutional_obligation_source.cpp
    src/resident/domains/organization/institutional_obligation_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/organization/organization_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_organization_organization_checkpoint_cpp
    src/resident/domains/organization/organization_checkpoint.cpp
    src/resident/domains/organization/organization_codec.cpp
    src/resident/domains/organization/organization_codec_detail.hpp
    src/resident/domains/organization/organization_internal.hpp
    src/resident/domains/organization/organization_model.hpp
    src/resident/domains/organization/organization_role_service_prefix.hpp
    src/resident/domains/organization/organization_member_prefix.hpp
    src/resident/domains/organization/organization_contract_authority.hpp
    src/resident/domains/organization/organization_rules.cpp
    src/resident/domains/organization/organization_link_not_implemented.cpp
    src/resident/domains/organization/named_membership_rules.cpp
    src/resident/domains/organization/named_membership_validation.cpp
    src/resident/domains/organization/membership_offer.cpp
    src/resident/domains/organization/organization_contract_authority.cpp
    src/resident/domains/organization/organization_validation.cpp
    src/resident/domains/organization/organization_views.cpp
)

# 生产身份输入连续登记段：src/resident/domains/organization/organization_office_history.hpp。
set(HANMO_RESIDENT_IDENTITY_organization_organization_office_history_hpp
    src/resident/domains/organization/organization_office_history.hpp
)

# 生产身份输入连续登记段：src/resident/domains/organization/authority_basis.cpp。
set(HANMO_RESIDENT_IDENTITY_organization_authority_basis_cpp
    src/resident/domains/organization/authority_basis.cpp
    src/resident/domains/organization/authority_basis_codec.cpp
    src/resident/domains/organization/authority_change_read.cpp
    src/resident/domains/organization/authority_registry.cpp
    src/resident/domains/organization/grant_authority.cpp
    src/resident/domains/organization/grant_terms_definition.cpp
    src/resident/domains/organization/grant_terms_definition.hpp
    src/resident/domains/organization/membership_office_transition.cpp
    src/resident/domains/organization/office_authority.cpp
    src/resident/domains/organization/office_lifecycle.cpp
    src/resident/domains/organization/organization_activity_history.cpp
    src/resident/domains/organization/organization_authority_codec.cpp
    src/resident/domains/organization/organization_calendar.cpp
    src/resident/domains/organization/organization_fact_source.cpp
    src/resident/domains/organization/organization_lifecycle_catalog.cpp
    src/resident/domains/organization/organization_lifecycle_contract.cpp
    src/resident/domains/organization/organization_original_runtime.cpp
    src/resident/domains/organization/organization_runtime.cpp
    src/resident/domains/organization/organization_runtime_codec.cpp
    src/resident/domains/organization/organization_authored_origin.cpp
    src/resident/domains/organization/organization_authored_lifecycle.cpp
    src/resident/domains/organization/organization_resource_validation_reads.cpp
    src/resident/domains/organization/organization_resource_validation_reads.hpp
    src/resident/domains/organization/organization_authored_lifecycle.hpp
    src/resident/domains/organization/organization_authored_initialization.hpp
    src/resident/domains/organization/organization_authored_responsibilities.hpp
    src/resident/domains/organization/organization_authored_fixed_origin.hpp
    src/resident/domains/organization/organization_authored_duty.hpp
    src/resident/domains/organization/organization_authored_base.hpp
    src/resident/domains/organization/organization_runtime_body_codec.cpp
    src/resident/domains/organization/organization_service_effects_codec.cpp
    src/resident/domains/organization/political_office_transition.cpp
    src/resident/domains/organization/public_market_organization.cpp
)

# 生产身份输入连续登记段：src/resident/domains/organization/organization_activity_policy.cpp。
set(HANMO_RESIDENT_IDENTITY_organization_organization_activity_policy_cpp
    src/resident/domains/organization/organization_activity_policy.cpp
    src/resident/domains/organization/organization_activity_policy_codec.cpp
    src/resident/domains/organization/organization_calendar_admission.cpp
    src/resident/domains/organization/organization_population_training_source.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/organization/source_owned_fields/domain_fields.inc
    src/resident/domains/organization/world_codec/catalog_runtime.inc
    src/resident/domains/organization/world_codec/checkpoint_read_organization.inc
    src/resident/domains/organization/world_codec/checkpoint_write_runtime.inc
    src/resident/domains/organization/world_codec/export_runtime.inc
    src/resident/domains/organization/world_codec/seed_dynamic_runtime.inc
    src/resident/domains/organization/world_codec/seed_original_payloads.inc
    src/resident/domains/organization/world_codec/seed_runtime_selection.inc
)
