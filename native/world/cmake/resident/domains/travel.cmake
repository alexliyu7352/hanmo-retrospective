# 旅行与空间：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/travel/travel_release_capture.cpp。
set(HANMO_RESIDENT_SOURCES_travel_travel_release_capture_cpp
    src/resident/domains/travel/travel_release_capture.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/travel_navigation.cpp。
set(HANMO_RESIDENT_SOURCES_travel_travel_navigation_cpp
    src/resident/domains/travel/travel_navigation.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/route_contact.cpp。
set(HANMO_RESIDENT_SOURCES_travel_route_contact_cpp
    src/resident/domains/travel/route_contact.cpp
    src/resident/domains/travel/route_geometry.cpp
    src/resident/domains/travel/route_travel.cpp
    src/resident/domains/travel/travel.cpp
    src/resident/domains/travel/travel_codec.cpp
    src/resident/domains/travel/travel_catalog.cpp
    src/resident/domains/travel/travel_movement.cpp
    src/resident/domains/travel/travel_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/actor_spatial_history.cpp。
set(HANMO_RESIDENT_SOURCES_travel_actor_spatial_history_cpp
    src/resident/domains/travel/actor_spatial_history.cpp
    src/resident/domains/travel/actor_spatial_read.cpp
    src/resident/domains/travel/cargo_handling.cpp
    src/resident/domains/travel/cargo_handling_action_time.cpp
    src/resident/domains/travel/cargo_handling_author.cpp
    src/resident/domains/travel/cargo_handling_json.cpp
    src/resident/domains/travel/cargo_handling_phase.cpp
    src/resident/domains/travel/group_journey_funding_codec.cpp
    src/resident/domains/travel/group_journey_terminal.cpp
    src/resident/domains/travel/group_movement.cpp
    src/resident/domains/travel/group_transport_registration.cpp
    src/resident/domains/travel/household_transport_presence.cpp
    src/resident/domains/travel/movement_action_time.cpp
    src/resident/domains/travel/movement_contact_json.cpp
    src/resident/domains/travel/movement_control.cpp
    src/resident/domains/travel/movement_encounter_integration.cpp
    src/resident/domains/travel/movement_exact_codec.cpp
    src/resident/domains/travel/movement_physical_basis.cpp
    src/resident/domains/travel/transport_manifest_restore.cpp
    src/resident/domains/travel/transport_payment_physical.cpp
    src/resident/domains/travel/transport_physical_fact_headers.cpp
    src/resident/domains/travel/transport_physical_ownership.cpp
    src/resident/domains/travel/transport_section_codec.cpp
    src/resident/domains/travel/transport_section_context.cpp
    src/resident/domains/travel/movement_exact_contact.cpp
    src/resident/domains/travel/movement_exact_continuation.cpp
    src/resident/domains/travel/movement_exact_geometry.cpp
    src/resident/domains/travel/movement_exact_phase.cpp
    src/resident/domains/travel/movement_exact_runtime.cpp
    src/resident/domains/travel/movement_exact_validation.cpp
    src/resident/domains/travel/movement_remaining.cpp
    src/resident/domains/travel/movement_result_read.cpp
    src/resident/domains/travel/native_spatial_carrier_sources.cpp
    src/resident/domains/travel/patrol_checkpoint.cpp
    src/resident/domains/travel/patrol_checkpoint_json.cpp
    src/resident/domains/travel/population_spatial_history.cpp
    src/resident/domains/travel/spatial_carrier.cpp
    src/resident/domains/travel/spatial_location_capture.cpp
    src/resident/domains/travel/transport_authority_json.cpp
    src/resident/domains/travel/transport_manifest_history.cpp
    src/resident/domains/travel/travel_action_time_read.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/mobile_inventory_adoption.cpp。
set(HANMO_RESIDENT_SOURCES_travel_mobile_inventory_adoption_cpp
    src/resident/domains/travel/mobile_inventory_adoption.cpp
    src/resident/domains/travel/mobile_inventory_codec.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/inventory_spatial_history.cpp。
set(HANMO_RESIDENT_SOURCES_travel_inventory_spatial_history_cpp
    src/resident/domains/travel/inventory_spatial_history.cpp
    src/resident/domains/travel/spatial_initial_state.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/personal_movement_funding.cpp。
set(HANMO_RESIDENT_SOURCES_travel_personal_movement_funding_cpp
    src/resident/domains/travel/personal_movement_funding.cpp
    src/resident/domains/travel/native_party_movement_cost.cpp
    src/resident/domains/travel/movement_funded_plan.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/movement_command.cpp。
set(HANMO_RESIDENT_SOURCES_travel_movement_command_cpp
    src/resident/domains/travel/movement_command.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/group_boundary_completion.cpp。
set(HANMO_RESIDENT_SOURCES_travel_group_boundary_completion_cpp
    src/resident/domains/travel/group_boundary_completion.cpp
)

# 生产输入连续登记段：src/resident/domains/travel/actor_spatial_source_membership.cpp。
set(HANMO_RESIDENT_SOURCES_travel_actor_spatial_source_membership_cpp
    src/resident/domains/travel/actor_spatial_source_membership.cpp
    src/resident/domains/travel/birth_placement_native.cpp
    src/resident/domains/travel/birth_placement_settlement.cpp
    src/resident/domains/travel/care_dependent_placement.cpp
    src/resident/domains/travel/group_crossing_positive_read.cpp
    src/resident/domains/travel/group_road_passage_read.cpp
    src/resident/domains/travel/travel_dependent_chain.cpp
    src/resident/domains/travel/travel_dependent_codec.cpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/travel_contact_capture.hpp。
set(HANMO_RESIDENT_IDENTITY_travel_travel_contact_capture_hpp
    src/resident/domains/travel/travel_contact_capture.hpp
    src/resident/domains/travel/travel_plan_capture.hpp
    src/resident/domains/travel/travel_release_capture.cpp
    src/resident/domains/travel/travel_release_capture.hpp
    src/resident/domains/travel/travel_rescue_capture.hpp
    src/resident/domains/travel/travel_step_expense.hpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/travel_navigation.hpp。
set(HANMO_RESIDENT_IDENTITY_travel_travel_navigation_hpp
    src/resident/domains/travel/travel_navigation.hpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/travel_navigation.cpp。
set(HANMO_RESIDENT_IDENTITY_travel_travel_navigation_cpp
    src/resident/domains/travel/travel_navigation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/route_contact.cpp。
set(HANMO_RESIDENT_IDENTITY_travel_route_contact_cpp
    src/resident/domains/travel/route_geometry.hpp
    src/resident/domains/travel/travel_decode_detail.hpp
    src/resident/domains/travel/route_contact.cpp
    src/resident/domains/travel/route_geometry.cpp
    src/resident/domains/travel/route_travel.cpp
    src/resident/domains/travel/travel.cpp
    src/resident/domains/travel/travel.hpp
    src/resident/domains/travel/travel_codec.cpp
    src/resident/domains/travel/travel_catalog.cpp
    src/resident/domains/travel/travel_model.hpp
    src/resident/domains/travel/travel_movement.cpp
    src/resident/domains/travel/travel_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/actor_spatial_history.cpp。
set(HANMO_RESIDENT_IDENTITY_travel_actor_spatial_history_cpp
    src/resident/domains/travel/actor_spatial_history.cpp
    src/resident/domains/travel/actor_spatial_read.cpp
    src/resident/domains/travel/cargo_handling.cpp
    src/resident/domains/travel/cargo_handling_action_time.cpp
    src/resident/domains/travel/cargo_handling_author.cpp
    src/resident/domains/travel/cargo_handling_json.cpp
    src/resident/domains/travel/cargo_handling_phase.cpp
    src/resident/domains/travel/group_boundary_completion.cpp
    src/resident/domains/travel/group_journey_funding_codec.cpp
    src/resident/domains/travel/group_journey_terminal.cpp
    src/resident/domains/travel/group_movement.cpp
    src/resident/domains/travel/group_transport_registration.cpp
    src/resident/domains/travel/household_transport_presence.cpp
    src/resident/domains/travel/inventory_spatial_history.cpp
    src/resident/domains/travel/mobile_inventory_adoption.cpp
    src/resident/domains/travel/mobile_inventory_codec.cpp
    src/resident/domains/travel/movement_action_time.cpp
    src/resident/domains/travel/movement_command.cpp
    src/resident/domains/travel/movement_contact_json.cpp
    src/resident/domains/travel/movement_control.cpp
    src/resident/domains/travel/movement_encounter_integration.cpp
    src/resident/domains/travel/movement_exact_codec.cpp
    src/resident/domains/travel/movement_physical_basis.cpp
    src/resident/domains/travel/transport_manifest_restore.cpp
    src/resident/domains/travel/transport_payment_physical.cpp
    src/resident/domains/travel/transport_physical_fact_headers.cpp
    src/resident/domains/travel/transport_physical_ownership.cpp
    src/resident/domains/travel/transport_section_codec.cpp
    src/resident/domains/travel/transport_section_context.cpp
    src/resident/domains/travel/movement_exact_contact.cpp
    src/resident/domains/travel/movement_exact_continuation.cpp
    src/resident/domains/travel/movement_exact_geometry.cpp
    src/resident/domains/travel/movement_exact_phase.cpp
    src/resident/domains/travel/movement_exact_runtime.cpp
    src/resident/domains/travel/movement_exact_validation.cpp
    src/resident/domains/travel/movement_remaining.cpp
    src/resident/domains/travel/movement_result_read.cpp
    src/resident/domains/travel/native_spatial_carrier_sources.cpp
    src/resident/domains/travel/patrol_checkpoint.cpp
    src/resident/domains/travel/patrol_checkpoint_json.cpp
    src/resident/domains/travel/population_spatial_history.cpp
    src/resident/domains/travel/spatial_carrier.cpp
    src/resident/domains/travel/spatial_initial_state.cpp
    src/resident/domains/travel/spatial_location_capture.cpp
    src/resident/domains/travel/transport_authority_json.cpp
    src/resident/domains/travel/transport_manifest_history.cpp
    src/resident/domains/travel/travel_action_time_read.cpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/personal_movement_funding.cpp。
set(HANMO_RESIDENT_IDENTITY_travel_personal_movement_funding_cpp
    src/resident/domains/travel/personal_movement_funding.cpp
    src/resident/domains/travel/native_party_movement_cost.cpp
    src/resident/domains/travel/movement_funded_plan.cpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/personal_movement_funding.hpp。
set(HANMO_RESIDENT_IDENTITY_travel_personal_movement_funding_hpp
    src/resident/domains/travel/personal_movement_funding.hpp
    src/resident/domains/travel/movement_funded_plan_detail.hpp
    src/resident/domains/travel/native_party_movement_cost.hpp
)

# 生产身份输入连续登记段：src/resident/domains/travel/actor_spatial_source_membership.cpp。
set(HANMO_RESIDENT_IDENTITY_travel_actor_spatial_source_membership_cpp
    src/resident/domains/travel/actor_spatial_source_membership.cpp
    src/resident/domains/travel/birth_placement_native.cpp
    src/resident/domains/travel/birth_placement_settlement.cpp
    src/resident/domains/travel/care_dependent_placement.cpp
    src/resident/domains/travel/group_crossing_positive_read.cpp
    src/resident/domains/travel/group_road_passage_read.cpp
    src/resident/domains/travel/travel_dependent_chain.cpp
    src/resident/domains/travel/travel_dependent_codec.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/travel/source_owned_fields/domain_fields.inc
    src/resident/domains/travel/world_codec/catalog_travel.inc
    src/resident/domains/travel/world_codec/catalog_world_templates.inc
    src/resident/domains/travel/world_codec/checkpoint_read_travel.inc
    src/resident/domains/travel/world_codec/checkpoint_write_transport.inc
    src/resident/domains/travel/world_codec/export_physical_sections.inc
    src/resident/domains/travel/world_codec/seed_transport_registration.inc
    src/resident/domains/travel/world_codec/selection_physical_domains.inc
    src/resident/domains/travel/world_codec/transport_json.inc
)
