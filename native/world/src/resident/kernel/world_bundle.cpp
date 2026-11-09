#include "resident/domains/military/military_supply_lookup.hpp"
#include "native_group_member_handoffs.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "native_actor_social_occupancy.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "native_political_relations.hpp"
#include "native_field_passage.hpp"
#include "native_field_world_scope.hpp"
#include "common_action_occupation.hpp"
#include "exact_complete_world.hpp"
#include "exact_social_period.hpp"
#include "exact_time_world.hpp"
#include "json_decoded_world_seed.hpp"
#include "native_born_population_state.hpp"
#include "native_condition_application_world.hpp"
#include "native_control_continuation.hpp"
#include "native_director_catalog.hpp"
#include "native_economic_facility_roots.hpp"
#include "native_economic_obligation_sources.hpp"
#include "native_face_to_face_sources.hpp"
#include "native_family_birth_state.hpp"
#include "native_family_initial_sources.hpp"
#include "native_governance_programs.hpp"
#include "native_growth_permanent_world.hpp"
#include "native_growth_validation.hpp"
#include "native_interaction_catalog.hpp"
#include "native_life_age_world.hpp"
#include "native_military_battle_lifecycle.hpp"
#include "native_military_occupation.hpp"
#include "native_military_state.hpp"
#include "native_movement_boundary_catalog.hpp"
#include "native_movement_boundary_continuation_catalog.hpp"
#include "native_local_move_catalog.hpp"
#include "native_local_move_world.hpp"
#include "native_military_custody_command.hpp"
#include "native_organization_runtime.hpp"
#include "native_personal_battle_lifecycle.hpp"
#include "personal_integration.hpp"
#include "native_political_world_scope.hpp"
#include "native_private_shop_household_custody.hpp"
#include "native_public_supply_inspection.hpp"
#include "native_transport_catalog.hpp"
#include "resident/domains/travel/transport_physical_fact_headers.hpp"
#include "politics_receipt_routing.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1_state.hpp"
#include "native_character_injury_state.hpp"
#include "resident/domains/family/family_origin_v2.hpp"
#include "resident/domains/growth/profile_acquisition_world.hpp"
#include "resident/domains/player_plan/plan_profile.hpp"
#include "resident/domains/politics/takeover/politics_occupation_taxpayer.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "world_read_stamp.hpp"

#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "political_subordinate_appointment_director_world.hpp"
#include "political_office_resignation_response_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "political_governance_agency_intent_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/domains/personal/personal_simultaneous_outcomes.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "military_siege_engineering_integration.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "private_shop_liquidation_integration.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_attendance_world.hpp"
#include "military_supply_attrition_integration.hpp"
#include "military_rout_integration.hpp"
#include "resident/domains/military/military_rout_profile.hpp"
#include "resident/domains/military/military_command_shock_profile.hpp"
#include "resident/domains/military/military_garrison_rest_profile.hpp"
#include "resident/domains/military/military_combat_conditions_profile.hpp"
#include "resident/domains/military/military_supply_attrition_profile.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "military_recovery_integration.hpp"
#include "resident/domains/tasks/extensions/task_extension_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/party_consent/party_recruitment_consent.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset.hpp"
#include "resident/domains/military/military_field_command_profile.hpp"
#include "resident/domains/military/military_recovery_rules.hpp"
#include "resident/domains/military/military_recovery_profile.hpp"
#include "resident/domains/military/military_field_command_rules.hpp"
#include "resident/domains/military/military_officer_profile.hpp"
#include "resident/domains/military/military_officer_rules.hpp"
#include "resident/domains/tasks/task_authored_continuation_catalog.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff_profile.hpp"
#include "resident/domains/military/naval_fleet_schedule.hpp"
#include "political_takeover_execution_world.hpp"
#include "political_takeover_integration.hpp"
#include "resident/domains/politics/takeover/politics_occupation_access.hpp"
#include "occupation_fiscal_integration.hpp"
#include "resident/domains/politics/supply/politics_supply_authority.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp"
#include "treaty_passage_integration.hpp"
#include "resident/domains/politics/treaty/treaty_passage_director.hpp"
#include "resident/domains/tasks/envoy_task_version.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "economic_world_integration.hpp"
#include "resident/domains/politics/politics_director.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp"
#include "siege_integration.hpp"
#include "named_membership.hpp"
#include "player_plan_integration.hpp"
#include "resident/domains/player_plan/plan_binding.hpp"
// 同一typed候选持所有领域；复制后仅重绑短寿命只读服务，校验不导出或重解析JSON。
#include "world_bundle.hpp"
#include "resident/domains/organization/organization_resource_validation_reads.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/military/military_exact_validation.hpp"
#include "resident/domains/military/training_exact_model.hpp"
#include "native_transport_funding.hpp"
#include "native_production_cycles.hpp"
#include "native_social_selection.hpp"
#include "resident/domains/social/social_calendar_phase.hpp"
#include "world_copy_plan.hpp"
#include "decoded_world_seed.hpp"
#include "resident/domains/growth/growth_tactic_lesson_boundary.hpp"
#include "resident/domains/growth/growth_tactic_lesson_responsibilities.hpp"
#include "military_tactic_learning_integration.hpp"
#include "political_governance_director_world.hpp"
#include "political_shop_registry_director_integration.hpp"
#include "political_governance_continuation_world.hpp"
#include "resident/domains/politics/governance_continuation/political_governance_continuation_declarations.hpp"
#include "private_shop_integration.hpp"
#include "native_private_shop_operations.hpp"
#include "resident/domains/commerce/private_shop_operation_sources.hpp"
#include "political_supply_renewal_integration.hpp"
#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"
#include "resident/domains/politics/takeover/politics_takeover_profile.hpp"
#include "resident/domains/politics/takeover/politics_takeover_authority.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/military/military_actor_occupation.hpp"
#include "resident/domains/family/family_legacy_receipt.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/tasks/task_personal_advance_profile.hpp"
#include "resident/domains/tasks/task_personal_advance_rules.hpp"
#include "resident/domains/military/naval_validation.hpp"
#include "resident/domains/military/naval_battle_history.hpp"
#include "resident/domains/military/military_journey_termination.hpp"
#include "resident/domains/military/naval_codec.hpp"
#include "naval_integration.hpp"
#include "naval_gunnery_integration.hpp"
#include "military_commanded_damage_integration.hpp"
#include "military_command_resources_integration.hpp"
#include "resident/domains/military/military_command_resources_profile.hpp"
#include "resident/domains/military/military_sequential_profile.hpp"
#include "resident/domains/military/military_field_terrain_profile.hpp"
#include <charconv>
#include "resident/domains/politics/diplomacy/diplomacy_response_director.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_delegation_director.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "resident/domains/interaction/party_invitation_profile.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_missions.hpp"
#include "command_types.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/military/battle_request_world.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"
#include "resident/domains/tasks/task_offer_response_profile.hpp"
#include "resident/domains/local_economy/relief/public_relief_catalog_links.hpp"
#include "resident/domains/politics/politics_governance.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include "dynamic_integration.hpp"
#include "regional_integration.hpp"
#include "task_cargo_access.hpp"
#include "resident/domains/growth/growth_life_results.hpp"
#include "resident/domains/organization/organization_contract_authority.hpp"
#include "resident/domains/organization/organization_validation.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include <unordered_set>
namespace hanmo::resident {
namespace {
// 每次借当前完整候选重建具名政治读；局部access与phase活到同步调用返回，绝不捕获悬空引用。
PopulationRecruitmentOccupationRead
recruitment_occupation_reader(const WorldBundle &w,
                              std::optional<Integer> finish) {
  if (!w.political_takeover_enabled())
    return {};
  return [&w, finish](const StableId &garrison)
             -> Outcome<std::optional<PoliticalRecruitmentOccupation>> {
    std::optional<DynamicStepInput> phase;
    if (finish) {
      if (w.common.tick() == INT64_MAX || *finish != w.common.tick() + 1)
        return {false,
                {},
                common_failure("PopulationOccupationPhaseMismatch", garrison)
                    .error};
      phase = DynamicStepInput{w.common.tick(), *finish, {}, {}, {}};
    }
    auto original = PoliticalTakeoverWorldAccess::create(
        w, *w.catalog->political_takeover, w.political_takeover,
        phase ? &*phase : nullptr);
    if (!original.ok)
      return {false, {}, original.error};
    PoliticalOccupationAccess read(original.value->context());
    return read.recruitment_occupation(garrison);
  };
}
// 冷登记以原action或enrolled头为边界，政治适配器拥有完整源读的局部生命周期。
PopulationRecruitmentOccupationHistoryRead
recruitment_occupation_history_reader(const WorldBundle &w,
                                      std::optional<Integer> finish) {
  if (!w.political_takeover_enabled())
    return {};
  return [&w, finish](const StableId &garrison, Integer tick,
                      const StableId &fact)
             -> Outcome<std::optional<PoliticalRecruitmentOccupation>> {
    std::optional<DynamicStepInput> phase;
    if (finish) {
      if (w.common.tick() == INT64_MAX || *finish != w.common.tick() + 1)
        return {false,
                {},
                common_failure("PopulationOccupationPhaseMismatch", garrison)
                    .error};
      phase = DynamicStepInput{w.common.tick(), *finish, {}, {}, {}};
    }
    auto original = PoliticalTakeoverWorldAccess::create(
        w, *w.catalog->political_takeover, w.political_takeover,
        phase ? &*phase : nullptr);
    if (!original.ok)
      return {false, {}, original.error};
    PoliticalOccupationAccess read(original.value->context());
    return read.recruitment_occupation_before(garrison, tick, fact);
  };
}
// 全局头与域payload是一对一关系；分别通过局部validator不能证明二者没有分叉。
Status validate_payload_registry(const WorldBundle &w,
                                 ProjectionWriter *projection) {
  std::unordered_set<StableId> facts, actions;
  GovernanceTaskPayloadRegistry governance_payloads;
  EnvoyTaskPayloadRegistry envoy_payloads;
  if (w.envoy_tasks_enabled()) {
    if (!w.catalog->diplomacy || !w.catalog->organization)
      return common_failure("MissingEnvoyCatalog");
    DiplomaticMissionAccess missions(w.common, w.organization,
                                     *w.catalog->organization,
                                     *w.catalog->diplomacy, w.diplomacy);
    auto envoy_access = w.services.interaction_access();
    DiplomaticPresentRead report_context{w.common,
                                         w.organization,
                                         *w.catalog->organization,
                                         *w.catalog->diplomacy,
                                         w.diplomacy,
                                         w.interaction,
                                         *w.catalog->interaction,
                                         envoy_access};
    std::optional<DiplomaticReportAccess> report_source;
    if (w.envoy_tasks.version == 2) {
      if (!w.diplomacy.v2)
        return common_failure("MissingDiplomacyV2State");
      report_source.emplace(report_context, *w.diplomacy.v2);
    }
    auto checked = envoy_task_payload_registry(
        w.common, w.tasks, w.envoy_tasks, *w.catalog->common, missions,
        missions, report_source ? &*report_source : nullptr);
    if (!checked.ok)
      return {false, {}, checked.error};
    envoy_payloads = std::move(checked.value);
  }
  if (w.governance_tasks_enabled()) {
    auto checked =
        governance_task_payload_registry(w.common, w.tasks, w.governance_tasks);
    if (!checked.ok)
      return {false, {}, checked.error};
    governance_payloads = std::move(checked.value);
  }
  auto fact = [&](const StableId &id, const StableId &kind,
                  const StableId &domain) -> Status {
    auto p = w.common.state().facts.find(id);
    if (!p || p->id != id || p->kind != kind || p->domain != domain ||
        !facts.insert(id).second)
      return common_failure(kind == "terrain_journey"
                                ? "TerrainJourneyFactMismatch"
                                : "ResidentFactRegistryMismatch",
                            id);
    return common_success();
  };
  // P3 原完整同 World 校验已在 validate_impl 先行完成；这里从真实拥有根
  // 逐个登记三类正文，不因 politics 域或 Common 头存在就豁免孤儿事实。
  if (w.political_relations) {
    auto r = fact(w.political_relations->initialization.id,
                  "political_relations_initialized", "politics");
    if (!r.ok)
      return r;
    for (const auto &row : w.political_relations->battle_changes.rows()) {
      r = fact(row.first, "political_battle_relation_changed", "politics");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.political_relations->alliance_breaches.rows()) {
      r = fact(row.first, "political_alliance_breached", "politics");
      if (!r.ok)
        return r;
    }
  }
  // Field 原完整 entry/capture/政策历史门已先行；这里只认领真实决定正文。
  // 仍由同一个 fact 门核 Common 身份与唯一拥有，不能拿 politics 头作白名单。
  if (w.political_passage) {
    for (const auto &row : w.political_passage->decisions.rows()) {
      auto source = read_political_passage_decision(
          w.common, *w.political_passage,
          {"politics", "political_passage_decided", row.first});
      if (!source.ok)
        return {false, {}, std::move(source.error)};
      if (source.value != &row.second)
        return common_failure("ResidentFactRegistryMismatch", row.first);
      auto r = fact(row.first, "political_passage_decided", "politics");
      if (!r.ok)
        return r;
    }
  }
  // 最终 S1 物理段已有双向原头验证器；共享同一真实根，不复制 kind 表，
  // 更不把“属于 travel”当作存在正文。未选仍拒绝任何残留的精确事实。
  auto transport =
      native_transport_physical_roots(const_cast<WorldBundle &>(w));
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  if (transport.value) {
    auto checked =
        validate_transport_physical_fact_headers(w.common, *transport.value);
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : w.common.state().facts.rows())
    if (owns_transport_physical_fact_kind(row.second.domain, row.second.kind)) {
      if (!transport.value)
        return common_failure("TransportPhysicalFactStateRequired", row.first);
      auto checked = fact(row.first, row.second.kind, row.second.domain);
      if (!checked.ok)
        return checked;
    }
  // 两个独立的原附项在前序 layout 以真实表双向核过，不重复登记所引用的
  // Field/死亡/容量来源；只登记本行自己拥有的 Common 头。
  if (w.movement_exact)
    for (const auto &row : w.movement_exact->boundary_contacts.rows()) {
      auto checked = fact(row.first, "movement_boundary_contact", "travel");
      if (!checked.ok)
        return checked;
    }
  if (w.movement_exact)
    for (const auto &row : w.movement_exact->boundary_continuations.rows()) {
      auto checked = fact(row.first, "movement_boundary_continuation", "travel");
      if (!checked.ok)
        return checked;
    }
  if (w.movement_exact)
    for (const auto &row : w.movement_exact->local_moves.rows()) {
      auto checked = fact(row.first, "local_move", "travel");
      if (!checked.ok)
        return checked;
    }
  if (w.group_movement_executions) {
    for (const auto &row :
         w.group_movement_executions->contact_member_releases.rows()) {
      auto checked = fact(row.first, "group_contact_member_release", "travel");
      if (!checked.ok)
        return checked;
    }
    for (const auto &row :
         w.group_movement_executions->member_loss_handoffs.rows()) {
      auto checked = fact(row.first, "group_member_loss_handoff", "travel");
      if (!checked.ok)
        return checked;
    }
  }
  // 社交占用原源有双向完整读者；没有 typed transition 的头不能获得登记。
  // 原总校验稍后仍执行，本处先借同一 World owner，不制造初始占用假事实。
  auto occupancy = validate_native_actor_social_occupancy(w);
  if (!occupancy.ok)
    return occupancy;
  if (w.interaction.social_occupancy)
    for (const auto &row : w.interaction.social_occupancy->transitions.rows()) {
      auto checked = fact(row.first, "actor_social_occupancy_changed", "interaction");
      if (!checked.ok)
        return checked;
    }
  // 原供货读取源已由前序完整校验；这里只登记该 typed 表的唯一事实头。
  if (w.public_supply_inspection)
    for (const auto &row : w.public_supply_inspection->results.rows()) {
      if (row.first != row.second.id)
        return common_failure("PublicSupplyInspectionSourceMissing", row.first,
                              ErrorKind::internal);
      auto checked =
          fact(row.first, public_supply_inspection_source_kind, "interaction");
      if (!checked.ok)
        return checked;
    }
  // 法律仅有六种零时永久事实；每个实体必须由本域精确头验证，不凭domain豁免。
  auto legal_fact = [&](const StableId &id, const StableId &kind) -> Status {
    const auto *header = w.common.state().facts.find(id);
    if (!w.political_shop_licensing_enabled() || !header ||
        header->kind != kind)
      return common_failure("PoliticalShopPayloadMismatch", id);
    auto checked = validate_political_shop_licensing_fact_header(
        w.common, w.political_shop_licensing, *header);
    if (!checked.ok)
      return checked;
    return fact(id, kind, political_shop_licensing_module);
  };
  for (const auto &row : w.political_shop_licensing.registries.rows()) {
    auto checked = legal_fact(row.first, "political_shop_registry_initialized");
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : w.political_shop_licensing.warrants.rows()) {
    auto checked = legal_fact(row.first, "political_wanted_registered");
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : w.political_shop_licensing.releases.rows()) {
    auto checked = legal_fact(row.first, "political_wanted_released");
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : w.political_shop_licensing.clearances.rows()) {
    auto checked = legal_fact(row.first, "political_shop_clearance_recorded");
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : w.political_shop_licensing.permits.rows()) {
    auto checked = legal_fact(row.first, "political_shop_permit_issued");
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : w.political_shop_licensing.revocations.rows()) {
    auto checked = legal_fact(row.first, "political_shop_permit_revoked");
    if (!checked.ok)
      return checked;
  }
  for (const auto &row :
       w.political_shop_registry_successions.acceptances.rows()) {
    const auto *h = w.common.state().facts.find(row.first);
    if (!w.political_shop_registry_successions_enabled() || !h)
      return common_failure("PoliticalShopRegistryPayloadRequired", row.first);
    auto checked = validate_political_shop_registry_fact_header(
        w.common, w.political_shop_registry_successions, *h);
    if (!checked.ok)
      return checked;
    checked = fact(row.first, political_shop_registry_accept_kind,
                   political_shop_registry_succession_module);
    if (!checked.ok)
      return checked;
  }
  if (w.political_governance_continuations)
    for (const auto &row :
         w.political_governance_continuations->continuations.rows()) {
      const auto *h = w.common.state().facts.find(row.first);
      if (!w.catalog->political_governance_continuations || !h)
        return common_failure("GovernanceContinuationFactMismatch", row.first);
      auto checked = validate_political_governance_continuation_fact_header(
          w.common, *w.political_governance_continuations, *h);
      if (!checked.ok)
        return checked;
      checked = fact(row.first, political_governance_continue_kind,
                     political_governance_continuation_module);
      if (!checked.ok)
        return checked;
    }
  if (w.governance_invitation_replies)
    for (const auto &row : w.governance_invitation_replies->replies.rows()) {
      const auto *h = w.common.state().facts.find(row.first);
      if (!w.catalog->political_governance_invitation_replies || !h)
        return common_failure("GovernanceInvitationFactMismatch", row.first);
      auto checked = validate_governance_invitation_reply_fact_header(
          w.common, *w.governance_invitation_replies, *h);
      if (!checked.ok)
        return checked;
      checked = fact(row.first, governance_invitation_reply_fact_kind,
                     governance_invitation_reply_module);
      if (!checked.ok)
        return checked;
    }
  if (w.political_head_handovers) {
    std::vector<StableId> ids;
    for (const auto &row : w.political_head_handovers->offers.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_head_handovers->answers.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_head_handovers->withdrawals.rows())
      ids.push_back(row.first);
    for (const auto &id : ids) {
      const auto *h = w.common.state().facts.find(id);
      if (!h)
        return common_failure("PoliticalHeadHandoverFactMismatch", id);
      auto checked = validate_political_head_handover_fact_header(
          w.common, *w.political_head_handovers, *h);
      if (!checked.ok)
        return checked;
      checked = fact(id, h->kind, political_head_handover_domain);
      if (!checked.ok)
        return checked;
    }
  }
  if (w.political_successor_rounds) {
    std::vector<StableId> ids;
    for (const auto &row : w.political_successor_rounds->nominations.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_successor_rounds->answers.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_successor_rounds->withdrawals.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_successor_rounds->accessions.rows())
      ids.push_back(row.first);
    for (const auto &id : ids) {
      const auto *h = w.common.state().facts.find(id);
      if (!h)
        return common_failure("PoliticalSuccessorRoundFactMismatch", id);
      auto checked = validate_political_successor_round_fact_header(
          w.common, *w.political_successor_rounds, *h);
      if (!checked.ok)
        return checked;
      checked = fact(id, h->kind, political_successor_rounds_domain);
      if (!checked.ok)
        return checked;
    }
  }
  if (w.political_term_handovers) {
    std::vector<StableId> ids;
    for (const auto &row : w.political_term_handovers->offers.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_term_handovers->answers.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_term_handovers->withdrawals.rows())
      ids.push_back(row.first);
    for (const auto &id : ids) {
      const auto *h = w.common.state().facts.find(id);
      if (!h)
        return common_failure("PoliticalTermHandoverFactMismatch", id);
      auto checked = validate_political_term_handover_fact_header(
          w.common, *w.political_term_handovers, *h);
      if (!checked.ok)
        return checked;
      checked = fact(id, h->kind, political_term_handover_domain);
      if (!checked.ok)
        return checked;
    }
  }
  if (w.political_office_receptions) {
    std::vector<StableId> ids;
    for (const auto &row : w.political_office_receptions->assignments.rows())
      ids.push_back(row.first);
    for (const auto &id : ids) {
      const auto *h = w.common.state().facts.find(id);
      if (!h)
        return common_failure("PoliticalOfficeReceptionFactMismatch", id);
      auto checked = validate_political_office_reception_fact_header(
          w.common, *w.political_office_receptions, *h);
      if (!checked.ok)
        return checked;
      checked = fact(id, h->kind, political_office_reception_domain);
      if (!checked.ok)
        return checked;
    }
  }
  if (w.political_successions) {
    std::vector<StableId> ids;
    for (const auto &row :
         w.political_successions->nominations.nominations.rows())
      ids.push_back(row.first);
    for (const auto &row : w.political_successions->nominations.answers.rows())
      ids.push_back(row.first);
    for (const auto &row :
         w.political_successions->nominations.withdrawals.rows())
      ids.push_back(row.first);
    for (const auto &row :
         w.political_successions->accessions.accessions.rows())
      ids.push_back(row.first);
    for (const auto &id : ids) {
      const auto *h = w.common.state().facts.find(id);
      if (!h)
        return common_failure("PoliticalSuccessionFactMismatch", id);
      auto checked = validate_political_succession_fact_header(
          w.common, *w.political_successions, *h);
      if (!checked.ok)
        return checked;
      checked = fact(id, h->kind, political_succession_domain);
      if (!checked.ok)
        return checked;
    }
  }
  if (w.interaction.visit_messages) {
    for (const auto &row : w.interaction.visit_messages->posted.rows()) {
      auto r = fact(row.first, visit_message_posted_kind, "visit_messages");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.interaction.visit_messages->received.rows()) {
      auto r = fact(row.first, visit_message_received_kind, "visit_messages");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.interaction.visit_messages->withdrawn.rows()) {
      auto r = fact(row.first, visit_message_withdrawn_kind, "visit_messages");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : w.travel.journey_facts.rows()) {
    auto r = fact(row.first, "terrain_journey", "travel");
    if (!r.ok)
      return r;
  }
  for (const auto &fleet : w.naval.fleets) {
    auto r = fact(fleet.source_id, "naval_form", "military_naval");
    if (!r.ok)
      return r;
    if (fleet.status == "released") {
      r = fact(fleet.release_source_id, "naval_disband", "military_naval");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : w.naval.repairs.starts) {
    if (!w.naval_repairs_enabled())
      return common_failure("NavalRepairProfileRequired", row.id);
    auto r = fact(row.id, "naval_repair_start", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.repairs.results) {
    if (!w.naval_repairs_enabled())
      return common_failure("NavalRepairProfileRequired", row.id);
    auto r = fact(row.id, "naval_repair_result", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.navigation.voyages) {
    if (!w.naval_navigation_enabled())
      return common_failure("NavalNavigationProfileRequired", row.id);
    auto r = fact(row.id, "naval_voyage", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.navigation.steps) {
    if (!w.naval_navigation_enabled())
      return common_failure("NavalNavigationProfileRequired", row.id);
    auto r = fact(row.id, "naval_navigation_step", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.gunnery.preparations) {
    if (!w.naval_gunnery_enabled())
      return common_failure("NavalGunneryProfileRequired", row.id);
    auto r = fact(row.id, "naval_gunnery_preparation", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.contacts) {
    auto r = fact(row.id, "naval_contact", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.battles) {
    auto r = fact(row.frame.id, "naval_battle", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.plans) {
    auto r = fact(row.id, "naval_plan", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.rounds) {
    auto r = fact(row.id, "naval_round", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.captures) {
    auto r = fact(row.id, "naval_capture", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.declines) {
    auto r = fact(row.id, "naval_decline", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.battle.continuations) {
    auto r = fact(row.id, "naval_continue", "military_naval");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.garrison_rest.facts) {
    if (!w.military_garrison_rest_enabled())
      return common_failure("MilitaryGarrisonRestProfileRequired", row.id);
    auto r = fact(row.id, "military_garrison_rest", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military_tactic_learning.acquisitions) {
    if (!w.military_tactic_learning_enabled())
      return common_failure("MilitaryTacticLearningProfileRequired", row.id);
    auto r = fact(row.id, "military_tactic_learned", "military_tactics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military_tactic_learning.practices) {
    if (!w.military_tactic_learning_enabled())
      return common_failure("MilitaryTacticLearningProfileRequired", row.id);
    auto r =
        fact(row.id, "military_tactic_practice_completed", "military_tactics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.rout.entries) {
    if (!w.military_rout_enabled())
      return common_failure("MilitaryRoutProfileRequired", row.id);
    auto r = fact(row.id, "military_rout_entry", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.rout.rounds) {
    if (!w.military_rout_enabled())
      return common_failure("MilitaryRoutProfileRequired", row.id);
    auto r = fact(row.id, "military_rout_round", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.supply_attrition.facts) {
    if (!w.military_supply_attrition_enabled())
      return common_failure("MilitarySupplyAttritionProfileRequired", row.id);
    auto r = fact(row.id, "military_supply_attrition", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.recovery.facts) {
    if (!w.military_recovery_enabled())
      return common_failure("MilitaryRecoveryProfileRequired", row.id);
    auto r = fact(row.id, "military_wounded_recovery", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.recovery.battle_releases) {
    if (!w.military_recovery_enabled())
      return common_failure("MilitaryRecoveryProfileRequired", row.id);
    auto r = fact(row.id, "military_battle_released", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.recovery.siege_releases) {
    if (!w.military_recovery_enabled())
      return common_failure("MilitaryRecoveryProfileRequired", row.id);
    auto r = fact(row.id, "military_siege_released", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.field_commands) {
    auto r = fact(row.id, "military_field_command_assigned", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.officer_assignments) {
    auto r = fact(row.id, "military_officer_assignment", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.request_facts) {
    auto r = fact(row.id, "military_request", "military");
    if (!r.ok)
      return r;
  }
  // Complete 的 office/form 原正文在 ActionTime 根；旧粗粒度表仍独立登记。
  // 前序 exact validator 已证明源、阶段、实际时刻及新旧唯一拥有，资源/条件
  // 附项借同一个 start/terminal，不重复认领它们引用的头。
  if (w.military_action_time) {
    for (const auto &row : w.military_action_time->dynamic_action_facts.rows()) {
      auto r = fact(row.first, "military_action", "military");
      if (!r.ok)
        return r;
      if (row.second.terminal_source_ref) {
        const auto &source = *row.second.terminal_source_ref;
        r = fact(source.id, source.kind, source.domain);
        if (!r.ok)
          return r;
      }
    }
    for (const auto &campaign : w.military.campaigns)
      if (campaign.exact_acceptance) {
        const auto &source =
            campaign.exact_acceptance->actual_acceptance_source;
        auto r = fact(source.id, "campaign_accepted", "military");
        if (!r.ok)
          return r;
      }
  }
  for (const auto &row : w.military.action_facts) {
    auto r = fact(row.id, "military_action", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.supply_facts) {
    auto r = fact(row.id, "military_supply", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.arrival_facts) {
    auto r = fact(row.id, "campaign_arrival", "military");
    if (!r.ok)
      return r;
  }
  // 不同typed账表可共享原kind，但全局ID只能登记一次；所有持久军事来源都必须有真实payload。
  for (const auto &row : w.military.logistics_facts) {
    auto r = fact(row.id, "military_action", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.return_facts) {
    auto r = fact(row.id, "military_return", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.muster_facts) {
    auto r = fact(row.id, "military_muster", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.resignation_facts) {
    auto r = fact(row.id, "office_resignation", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.delivery_facts) {
    auto r = fact(row.id, "military_delivery", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.settlement_facts) {
    auto r = fact(row.id, "campaign_settlement", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.contact_facts) {
    auto r = fact(row.id, "military_contact", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.battle_facts) {
    auto r = fact(row.id, "military_battle", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.casualty_facts) {
    auto r = fact(row.id, "military_casualties", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.occupation_facts) {
    auto r = fact(row.id, "military_occupation", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.relief_facts) {
    auto r = fact(row.id, "military_relief", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.siege_material_facts) {
    auto r = fact(row.id, "siege_material", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.training_facts) {
    auto r = fact(row.id, "military_training", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.march_facts) {
    auto r = fact(row.id, "military_march", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.estate.facts.rows()) {
    auto kind = std::visit(
        [](const auto &f) -> StableId {
          using T = std::decay_t<decltype(f)>;
          if constexpr (std::is_same_v<T, EstateProductionFact>)
            return "estate_production";
          else
            return f.kind;
        },
        row.second);
    auto r = fact(row.first, kind, "estate");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.tasks.facts.rows()) {
    auto r = fact(row.first, "escort_arrival", "tasks");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.economic_supply_policies.policies.rows()) {
    auto r = fact(row.first, economic_supply_policy_issue_kind,
                  economic_supply_policy_module);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.economic_supply_policies.observations.rows()) {
    auto r = fact(row.first, economic_supply_observation_kind,
                  economic_supply_policy_module);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.estate_employment.contracts.rows()) {
    auto r = fact(row.first, "estate_employment_offered", "estate_employment");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.estate_employment.decisions.rows()) {
    auto r = fact(row.first, "estate_employment_decided", "estate_employment");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.estate_employment.services.rows()) {
    auto r = fact(row.first, "estate_employment_service", "estate_employment");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_voluntary_paths.cancellations.rows()) {
    auto r =
        fact(row.first, voluntary_path_cancelled_kind, voluntary_path_module);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_life.deaths.rows()) {
    auto r = fact(row.first, "family_death", "family_life");
    if (!r.ok)
      return r;
  }
  if (w.production_cycles) {
    for (const auto &row : w.production_cycles->cycles.rows()) {
      auto r = fact(row.first, "production_cycle_started", "local_economy");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.production_cycles->segments.rows()) {
      auto r = fact(row.first, "production_input_segment", "local_economy");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.production_cycles->period_openings.rows()) {
      auto r =
          fact(row.second.source_id, "economic_period_opened", "local_economy");
      if (!r.ok)
        return r;
    }
  }
  if (w.private_transport_funding)
    for (const auto &row : w.private_transport_funding->terms.rows()) {
      auto r = fact(row.first, "self_transport_funding_accepted", "commerce");
      if (!r.ok)
        return r;
    }
  if (w.life_age_checks) {
    for (const auto &row : w.life_age_checks->checks.rows()) {
      auto r = fact(row.first, "age_check", "family_life");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.life_age_checks->settings_changes.rows()) {
      auto r = fact(row.first, "life_age_settings_changed", "family_life");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &h : w.common.state().facts.rows())
    if (h.second.domain == "family_care_relief") {
      if (!w.family_care_relief_enabled())
        return common_failure("ChildCareReliefProfileRequired");
      auto r =
          validate_child_care_relief_fact_header(w.child_care_relief, h.second);
      if (!r.ok)
        return r;
      r = fact(h.first, h.second.kind, "family_care_relief");
      if (!r.ok)
        return r;
    }
  for (const auto &h : w.common.state().facts.rows())
    if (h.second.domain == "family_birth") {
      if (!w.family_birth_enabled())
        return common_failure("BirthProfileRequired");
      auto r = validate_birth_fact_header(w.family_birth, h.second);
      if (!r.ok)
        return r;
      r = fact(h.first, h.second.kind, "family_birth");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.family_apprenticeship.proposals.rows()) {
    auto r = fact(row.first, "family_apprenticeship_proposed",
                  "family_apprenticeship");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_apprenticeship.replies.rows()) {
    auto r = fact(row.first, "family_apprenticeship_replied",
                  "family_apprenticeship");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_apprenticeship.exits.rows()) {
    auto r = fact(row.first, "family_apprenticeship_exited",
                  "family_apprenticeship");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_marriage.proposals.rows()) {
    auto r = fact(row.first, "family_marriage_proposed", "family_marriage");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_marriage.replies.rows()) {
    auto r = fact(row.first, "family_marriage_replied", "family_marriage");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_marriage.dissolutions.rows()) {
    auto r = fact(row.first, "family_marriage_dissolved", "family_marriage");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_inheritance.disclosures.rows()) {
    auto r = fact(row.first, "family_bequest_disclosed", "family_inheritance");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_inheritance.acceptances.rows()) {
    auto r = fact(row.first, "family_bequest_accepted", "family_inheritance");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_inheritance.settlements.rows()) {
    auto r =
        fact(row.first, "family_inheritance_settled", "family_inheritance");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_bequest.declarations.rows()) {
    auto r = fact(row.first, "family_bequest_declaration", "family_bequest");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_bequest.revocations.rows()) {
    auto r = fact(row.first, "family_bequest_revocation", "family_bequest");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_legacy.consents.rows()) {
    auto r = fact(row.first, "family_legacy_control_consent", "family_legacy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_legacy.revocations.rows()) {
    auto r =
        fact(row.first, "family_legacy_control_revocation", "family_legacy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family_legacy.activations.rows()) {
    auto r =
        fact(row.first, "family_legacy_control_activated", "family_legacy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.family.facts.rows()) {
    auto kind = std::visit(
        [](const auto &value) -> StableId {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, IdentityCreationFact>)
            return "actor_creation";
          else
            return value.kind;
        },
        row.second);
    auto r = fact(row.first, kind, "family");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.personal.facts.rows()) {
    auto r = fact(row.first, "personal_battle", "personal");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.growth.facts.rows()) {
    auto kind = std::visit(
        [](const auto &value) -> StableId {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, TrainingFact>)
            return "training";
          else if constexpr (std::is_same_v<T, StudyFact>)
            return "skill_study";
          else if constexpr (std::is_same_v<T, RestFact>)
            return "long_rest";
          else
            return "trade_bargain";
        },
        row.second);
    auto r = fact(row.first, kind,
                  kind == "long_rest"       ? "services"
                  : kind == "trade_bargain" ? "economy"
                                            : "cultivation");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.growth_tactic_lessons.starts.rows()) {
    auto r = fact(row.first, "tactic_lesson_started", "growth_tactic_lessons");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.growth_tactic_lessons.credits.rows()) {
    auto r = fact(row.first, "tactic_lesson_credit", "growth_tactic_lessons");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.growth_tactic_lessons.closures.rows()) {
    auto r = fact(row.first, "tactic_lesson_closed", "growth_tactic_lessons");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.personal_recovery.credits.rows()) {
    auto r =
        fact(row.first, "personal_recovery_credit", "personal_injury_recovery");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.personal_recovery.closures.rows()) {
    auto r =
        fact(row.first, "personal_recovery_closed", "personal_injury_recovery");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.growth.life_activities.rows()) {
    auto r = fact(row.first, "life_activity", "growth");
    if (!r.ok)
      return r;
  }
  // 原终态发表有独立真表；只逐头登记，不以 growth 域豁免缺失正文。
  for (const auto &row : w.growth.terminal_publications.rows()) {
    if (row.first != row.second.publication_id)
      return common_failure("ResidentFactRegistryMismatch", row.first);
    auto registered = fact(row.first, "growth_result_terminal", "growth");
    if (!registered.ok)
      return registered;
  }
  if (w.dynamic_enabled()) {
    for (const auto &row : w.local_economy.arrears.rows()) {
      const auto *obligation =
          w.local_economy.obligations.find(row.second.obligation_id);
      if (!obligation)
        return common_failure("MissingArrearObligation", row.first);
      auto r = fact(row.first,
                    obligation->kind == ObligationKind::wage
                        ? "wage_due"
                        : "contract_payment_due",
                    "local_economy");
      if (!r.ok)
        return r;
    }
    auto economy_facts = [&](const auto &table,
                             const StableId &kind) -> Status {
      for (const auto &row : table.rows()) {
        auto r = fact(row.first, kind, "local_economy");
        if (!r.ok)
          return r;
      }
      return common_success();
    };
    for (auto r :
         {economy_facts(w.local_economy.payments, "obligation_payment"),
          economy_facts(w.local_economy.rations, "local_ration"),
          economy_facts(w.local_economy.production_receipts,
                        "local_production"),
          economy_facts(w.local_economy.population_transfers,
                        "population_transfer"),
          economy_facts(w.local_economy.population_losses, "population_loss"),
          economy_facts(w.local_economy.supply_registrations,
                        "local_supply_source"),
          economy_facts(w.local_economy.supply_observations,
                        "local_supply_observation")})
      if (!r.ok)
        return r;
    if (w.local_economy.production_management) {
      for (auto r :
           {economy_facts(w.local_economy.production_management->configurations,
                          "production_configuration_requested")})
        if (!r.ok)
          return r;
      for (const auto &row :
           w.local_economy.production_management->results.rows()) {
        auto r = fact(row.first, "production_configuration_" + row.second.kind,
                      "local_economy");
        if (!r.ok)
          return r;
      }
    }
    if (w.local_economy.household_provisioning) {
      for (auto r :
           {economy_facts(w.local_economy.household_provisioning->registrations,
                          "household_provision_registered"),
            economy_facts(w.local_economy.household_provisioning->closures,
                          "household_provision_ended")})
        if (!r.ok)
          return r;
    }
    if (household_provision_management_enabled(w)) {
      auto r = economy_facts(w.household_provision_management.configurations,
                             "household_provision_configured");
      if (!r.ok)
        return r;
      for (const auto &row :
           w.household_provision_management.resolutions.rows()) {
        r = fact(row.first,
                 "household_provision_configuration_" + row.second.kind,
                 "local_economy");
        if (!r.ok)
          return r;
      }
    }
    if (w.production_disruptions_enabled())
      for (auto checked : {economy_facts(w.production_disruptions.registrations,
                                         "production_disruption_registered"),
                           economy_facts(w.production_disruptions.settlements,
                                         "production_disruption_settled")})
        if (!checked.ok)
          return checked;
    if (w.fiscal_settlement_enabled()) {
      for (const auto &row : w.fiscal_settlement.districts.rows()) {
        auto r = fact(row.second.registration_fact_id,
                      "fiscal_source_registered", "local_economy");
        if (!r.ok)
          return r;
      }
      for (auto r :
           {economy_facts(w.fiscal_settlement.periods,
                          "fiscal_period_assessed"),
            economy_facts(w.fiscal_settlement.assessments,
                          "fiscal_source_assessed"),
            economy_facts(w.fiscal_settlement.payments, "fiscal_payment")})
        if (!r.ok)
          return r;
    }
    if (w.public_relief_enabled()) {
      for (auto result :
           {economy_facts(w.public_relief.commitments,
                          "public_relief_reserved"),
            economy_facts(w.public_relief.receipts,
                          "public_relief_distributed"),
            economy_facts(w.public_relief.releases, "public_relief_released")})
        if (!result.ok)
          return result;
    }
    for (const auto &row :
         w.local_economy.membership_terminal_fractions.rows()) {
      auto r = fact(row.first, "membership_wage_ended", "local_economy");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.organization.facts.rows()) {
      auto r = fact(row.first, row.second.kind, "organization");
      if (!r.ok)
        return r;
    }
    // 原完整政治闭包已反验作者起源；这里只登记同一 runtime
    // 真列，不豁免整个域。
    if (w.organization_runtime) {
      for (const auto &row : w.organization_runtime->lifecycle_origins.rows()) {
        if (row.first != row.second.id)
          return common_failure("ResidentFactRegistryMismatch", row.first);
        if (!std::holds_alternative<OrganizationAuthoredOrigin>(
                row.second.origin))
          return common_failure("NOT_IMPLEMENTED", row.first,
                                ErrorKind::unsupported_capability);
        auto registered =
            fact(row.first, "organization_lifecycle_origin", "organization");
        if (!registered.ok)
          return registered;
      }
    }
    if (w.tasks.personal_advances) {
      for (const auto &row : w.tasks.personal_advances->payments.rows()) {
        auto r = fact(row.first, task_advance_fact_kind, "tasks");
        if (!r.ok)
          return r;
      }
      for (const auto &row : w.tasks.personal_advances->repayments.rows()) {
        auto r = fact(row.first, task_advance_repayment_fact_kind, "tasks");
        if (!r.ok)
          return r;
      }
    }
    for (const auto &row : w.tasks.transfers.rows()) {
      auto r = fact(row.first, "task_transfer", "tasks");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.tasks.deliveries.rows()) {
      auto r = fact(row.first, "task_delivery", "tasks");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.tasks.cargo_dispositions.rows()) {
      auto r = fact(row.first, "task_cargo_disposition", "tasks");
      if (!r.ok)
        return r;
    }
  }
  if (w.population_military_enabled()) {
    for (const auto &row : w.population_military.action_facts.rows()) {
      auto r =
          fact(row.first, "population_military_action", "population_military");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.population_military.facts.rows()) {
      auto r = fact(row.first, row.second.kind, "population_military");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : w.military.siege_integration.proposals) {
    auto r = fact(row.id, "siege_integration_proposal", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.siege_integration.actor_actions) {
    auto r = fact(row.id, "siege_integration_actor_action", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.siege_integration.decisions) {
    auto r = fact(row.id, "siege_officer_decision", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.integration_facts) {
    auto r = fact(row.id, "military_integration", "military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.population_military.integration_facts.rows()) {
    auto r = fact(row.first, "population_military_integration",
                  "population_military");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.political_takeover.registrations.rows()) {
    auto r = fact(row.first, political_takeover_pending_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.political_takeover.approvals.rows()) {
    auto r = fact(row.first, political_takeover_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.political_takeover.decisions.rows()) {
    auto r = fact(row.first, political_takeover_decision_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  if (w.tasks.takeover_execution)
    for (const auto &row : w.tasks.takeover_execution->payments.rows()) {
      auto r = fact(row.first, task_contract_payment_fact_kind, "tasks");
      if (!r.ok)
        return r;
    }
  if (w.tasks.message_handoffs)
    for (const auto &row : w.tasks.message_handoffs->receipts.rows()) {
      auto r = fact(row.first, task_message_handoff_kind, "tasks");
      if (!r.ok)
        return r;
    }
  if (w.tasks.message_notices)
    for (const auto &row : w.tasks.message_notices->bindings.rows()) {
      auto r = fact(row.first, task_death_notice_bound_kind, "tasks");
      if (!r.ok)
        return r;
    }
  if (w.tasks.static_assets)
    for (const auto &row : w.tasks.static_assets->receipts.rows()) {
      auto r = fact(row.first, task_static_asset_fact_kind, "tasks");
      if (!r.ok)
        return r;
    }
  if (w.tasks.deadline_extensions)
    for (const auto &row : w.tasks.deadline_extensions->facts.rows()) {
      auto r = fact(row.first, task_extension_fact_kind, task_extension_module);
      if (!r.ok)
        return r;
    }
  if (w.interaction.personal_appointments) {
    auto payloads = personal_appointment_fact_payloads(
        w.common, &*w.interaction.personal_appointments);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "personal_appointments")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               personal_appointment_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = contact_inquiry_fact_payloads(
        w.common, w.interaction.contact_inquiries
                      ? &*w.interaction.contact_inquiries
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "contact_inquiries")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               contact_inquiry_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = visit_reply_fact_payloads(
        w.common, w.interaction.visit_message_replies
                      ? &*w.interaction.visit_message_replies
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "visit_message_replies")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               visit_reply_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = born_personal_dialogue_fact_payloads(
        w.common, w.interaction.born_personal_dialogues
                      ? &*w.interaction.born_personal_dialogues
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "born_personal_dialogues")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               born_personal_dialogue_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = known_personal_dialogue_fact_payloads(
        w.common, w.interaction.known_personal_dialogues
                      ? &*w.interaction.known_personal_dialogues
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "known_personal_dialogues")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               known_personal_dialogue_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_shop_order_contact_fact_payloads(
        w.common, w.tasks.shop_orders && w.tasks.shop_orders->contacts
                      ? &*w.tasks.shop_orders->contacts
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "task_shop_order_contacts")) {
      auto r = fact(row.first, task_shop_order_contact_started_kind,
                    task_shop_order_contact_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_message_return_fact_payloads(
        w.common,
        w.tasks.message_returns ? &*w.tasks.message_returns : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "task_message_returns")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               task_message_return_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_personal_message_fact_payloads(
        w.common,
        w.tasks.personal_messages ? &*w.tasks.personal_messages : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "task_personal_message_deliveries")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               task_personal_message_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_personal_report_fact_payloads(
        w.common,
        w.tasks.personal_reports ? &*w.tasks.personal_reports : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "task_personal_message_reports")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               task_personal_report_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_personal_acceptance_fact_payloads(
        w.common, w.tasks.personal_acceptances ? &*w.tasks.personal_acceptances
                                               : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row : boundary::object(
             payloads.value, "task_personal_message_acceptances")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               task_personal_acceptance_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_personal_cancellation_fact_payloads(
        w.common, w.tasks.personal_cancellations
                      ? &*w.tasks.personal_cancellations
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row : boundary::object(
             payloads.value, "task_personal_message_cancellations")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               task_personal_cancellation_module);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = collateral_recognition_fact_payloads(
        w.common, w.family_collateral_legacy_enabled()
                      ? &w.family_collateral_legacy
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "family_collateral_legacy")) {
      auto r = fact(row.first, "family_collateral_recognized",
                    "family_collateral_legacy");
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = visit_appointment_fact_payloads(
        w.common,
        w.interaction.visit_appointment_invitations
            ? &*w.interaction.visit_appointment_invitations
            : nullptr,
        w.interaction.visit_appointment_meetings
            ? &*w.interaction.visit_appointment_meetings
            : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "visit_appointments")) {
      auto r =
          fact(row.first, boundary::get_string(row.second, "kind", row.first),
               boundary::get_string(row.second, "domain", row.first));
      if (!r.ok)
        return r;
    }
  }
  if (w.tasks.message_handoffs && (w.tasks.message_handoffs->version == 2 ||
                                   w.tasks.message_handoffs->version == 3 ||
                                   w.tasks.message_handoffs->version == 4))
    for (const auto &row : w.tasks.message_handoffs->reports.rows()) {
      auto r = fact(row.first, task_message_report_kind, "tasks");
      if (!r.ok)
        return r;
    }
  if (w.tasks.takeover_execution)
    for (const auto &row : w.tasks.takeover_execution->static_payments.rows()) {
      auto r = fact(row.first, task_static_payment_fact_kind, "tasks");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.politics.facts.rows()) {
    auto r = fact(row.first, row.second.kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.diplomacy.facts.rows()) {
    auto r = fact(row.first, row.second.kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.diplomacy.missions.rows()) {
    auto r =
        fact(row.first, diplomatic_mission_authority_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  if (w.diplomacy.v2) {
    for (const auto &row : w.diplomacy.v2->mission_knowledge.rows()) {
      auto r = fact(row.first, diplomatic_mission_known_fact_kind, "politics");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.diplomacy.v2->reports.rows()) {
      auto r = fact(row.first, diplomatic_envoy_report_fact_kind, "politics");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : w.treaty_passage.deliveries.rows()) {
    auto r = fact(row.first, treaty_passage_delivery_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.treaty_passage.signatures.rows()) {
    auto r = fact(row.first, treaty_passage_signature_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &r :
       w.political_military_appointments.authorizations.rows()) {
    auto v = fact(r.first, political_officer_issue_kind, "politics");
    if (!v.ok)
      return v;
  }
  for (const auto &r : w.political_field_appointments.authorizations.rows()) {
    auto v = fact(r.first, political_field_issue_kind, "politics");
    if (!v.ok)
      return v;
  }
  for (const auto &r : w.political_military_appointments.answers.rows()) {
    auto v = fact(r.first,
                  r.second.accepted ? political_officer_accept_kind
                                    : political_officer_decline_kind,
                  "politics");
    if (!v.ok)
      return v;
  }
  for (const auto &r : w.political_field_appointments.answers.rows()) {
    auto v = fact(r.first,
                  r.second.accepted ? political_field_accept_kind
                                    : political_field_decline_kind,
                  "politics");
    if (!v.ok)
      return v;
  }
  for (const auto &r : w.political_military_appointments.withdrawals.rows()) {
    auto v = fact(r.first, political_officer_withdraw_kind, "politics");
    if (!v.ok)
      return v;
  }
  for (const auto &r : w.political_field_appointments.withdrawals.rows()) {
    auto v = fact(r.first, political_field_withdraw_kind, "politics");
    if (!v.ok)
      return v;
  }
  for (const auto &row : w.occupation_fiscal.policies.rows()) {
    auto r = fact(row.first, political_occupation_fiscal_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row :
       w.occupation_fiscal_settlement.suspended_periods.rows()) {
    auto r =
        fact(row.first, occupation_fiscal_suspension_kind, "local_economy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.occupation_fiscal_settlement.periods.rows()) {
    auto r = fact(row.first, occupation_fiscal_period_kind, "local_economy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.occupation_fiscal_settlement.assessments.rows()) {
    auto r =
        fact(row.first, occupation_fiscal_assessment_kind, "local_economy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.occupation_fiscal_settlement.payments.rows()) {
    auto r = fact(row.first, occupation_fiscal_payment_kind, "local_economy");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.political_fiscal.policies.rows()) {
    auto r = fact(row.first, "political_fiscal_policy_changed", "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : governance_payloads.facts) {
    auto r = fact(row.id, row.kind, row.domain);
    if (!r.ok)
      return r;
  }
  if (w.political_governance_agencies) {
    auto collect = [&](const auto &table) -> Status {
      for (const auto &row : table.rows()) {
        const auto *h = w.common.state().facts.find(row.first);
        if (!w.catalog->political_governance_agencies || !h)
          return common_failure("PoliticalGovernanceAgencyFactMismatch",
                                row.first);
        auto v = validate_political_governance_agency_fact_header(
            w.common, *w.political_governance_agencies, *h);
        if (!v.ok)
          return v;
        v = fact(row.first, h->kind, political_governance_agency_domain);
        if (!v.ok)
          return v;
      }
      return common_success();
    };
    for (const auto &status :
         {collect(w.political_governance_agencies->offers),
          collect(w.political_governance_agencies->answers),
          collect(w.political_governance_agencies->revocations),
          collect(w.political_governance_agencies->issues)})
      if (!status.ok)
        return status;
  }
  if (w.political_office_resignations) {
    auto collect = [&](const auto &table) -> Status {
      for (const auto &row : table.rows()) {
        const auto *h = w.common.state().facts.find(row.first);
        if (!w.catalog->political_office_resignations || !h)
          return common_failure("PoliticalOfficeResignationFactMismatch",
                                row.first);
        auto v = validate_political_office_resignation_fact_header(
            w.common, *w.political_office_resignations, *h);
        if (!v.ok)
          return v;
        v = fact(row.first, h->kind, political_office_resignation_module);
        if (!v.ok)
          return v;
      }
      return common_success();
    };
    for (const auto &v :
         {collect(w.political_office_resignations->requests),
          collect(w.political_office_resignations->answers),
          collect(w.political_office_resignations->withdrawals)})
      if (!v.ok)
        return v;
  }
  if (w.political_subordinate_appointments) {
    auto collect = [&](const auto &table) -> Status {
      for (const auto &row : table.rows()) {
        const auto *h = w.common.state().facts.find(row.first);
        if (!w.catalog->political_subordinate_appointments || !h)
          return common_failure("PoliticalSubordinateAppointmentFactMismatch",
                                row.first);
        auto v = validate_political_subordinate_appointment_fact_header(
            w.common, *w.political_subordinate_appointments, *h);
        if (!v.ok)
          return v;
        v = fact(row.first, h->kind, political_subordinate_appointment_module);
        if (!v.ok)
          return v;
      }
      return common_success();
    };
    for (const auto &v :
         {collect(w.political_subordinate_appointments->offers),
          collect(w.political_subordinate_appointments->answers),
          collect(w.political_subordinate_appointments->withdrawals),
          collect(w.political_subordinate_appointments->arrivals)})
      if (!v.ok)
        return v;
  }
  for (const auto &row : w.politics_governance.authorities.rows()) {
    auto r = fact(row.first, governance_authority_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.politics_governance.plans.rows()) {
    auto r = fact(row.first, governance_outcome_fact_kind, "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.politics_governance.civil.rows()) {
    auto r = fact(row.second.registration_fact_id, "political_civil_registered",
                  "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.politics_governance.consequences.rows()) {
    auto r = fact(row.first, "political_civil_consequence", "politics");
    if (!r.ok)
      return r;
  }
  for (const auto &row : envoy_payloads.facts) {
    auto r = fact(row.id, row.kind, row.domain);
    if (!r.ok)
      return r;
  }
  // 原 Shop2 三类持值仍复用原 paired payload；labor 独立头不能落旧基础表。
  auto shop_selection =
      selected_native_private_shop_operation_versions(w.catalog->identity);
  if (!shop_selection.ok)
    return {false, {}, std::move(shop_selection.error)};
  if (bool(w.private_shop_operation_sources) !=
          shop_selection.value.exact_operations() ||
      bool(w.private_shop_operation_capabilities) !=
          shop_selection.value.exact_operations())
    return common_failure("ShopOperationSelectedRootMismatch");
  for (const auto &row : w.common.state().facts.rows())
    if (row.second.domain == "private_shops" ||
        (row.second.kind.rfind("private_shop_", 0) == 0 &&
         row.second.domain != "private_shop_liquidations" &&
         !(row.second.domain == private_shop_daily_rent_module &&
           (row.second.kind == private_shop_rent_assessed_kind ||
            row.second.kind == private_shop_rent_paid_kind ||
            row.second.kind == private_shop_rent_resumed_kind)))) {
      if (!w.private_shops_enabled())
        return common_failure("PrivateShopProfileRequired", row.first);
      Integer schema = 1;
      if (shop_selection.value.exact_operations()) {
        auto original = read_private_shop_bootstrap_payload_schema(
            w.common, w.private_shops, *w.private_shop_operation_sources,
            *w.private_shop_operation_capabilities, row.second);
        if (!original.ok)
          return {false, {}, std::move(original.error)};
        schema = original.value;
      }
      Status r = common_success();
      if (shop_selection.value.exact_operations() &&
          (schema == 2 || row.second.kind == "private_shop_labor_applied")) {
        auto emit = [&](ProjectionWriter &writer) {
          return emit_private_shop_operation_source_payload_fields(
              writer, w.common, w.private_shops,
              *w.private_shop_operation_sources, row.second, schema);
        };
        if (projection)
          r = projection->validate_detached(emit);
        else {
          BoundaryValue payload;
          BoundaryProjectionWriter writer(payload);
          r = writer.object("", emit);
          if (r.ok)
            r = writer.finish();
        }
      } else
        r = projection
                ? validate_private_shop_fact_header(w.common, w.private_shops,
                                                    row.second, *projection)
                : validate_private_shop_fact_header(w.common, w.private_shops,
                                                    row.second);
      if (!r.ok)
        return r;
      r = fact(row.first, row.second.kind, "private_shops");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.growth_market_history.movements.rows()) {
    if (!w.growth_market_history_enabled() ||
        row.second.kind != "growth_market_movement")
      return common_failure("GrowthMarketHistoryProfileRequired", row.first);
    auto r = fact(row.first, "growth_market_movement", "growth_market_history");
    if (!r.ok)
      return r;
  }
  if (w.tasks.shop_orders) {
    for (const auto &row : w.tasks.shop_orders->bindings.rows()) {
      auto r = fact(row.first, "task_shop_order_accepted", "task_shop_orders");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.tasks.shop_orders->settlements.rows()) {
      auto r = fact(row.first, "task_shop_order_settled", "task_shop_orders");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.tasks.shop_orders->interruptions.rows()) {
      auto r =
          fact(row.first, "task_shop_order_interrupted", "task_shop_orders");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : w.common.state().facts.rows())
    if (row.second.domain == "private_shop_liquidations") {
      if (!w.private_shop_liquidations_enabled())
        return common_failure("PrivateShopLiquidationProfileRequired",
                              row.first);
      auto payload = encode_private_shop_liquidation_fact_payload(
          w.common, w.private_shop_liquidations, row.second);
      if (!payload.ok)
        return {false, {}, payload.error};
      auto r = fact(row.first, row.second.kind, "private_shop_liquidations");
      if (!r.ok)
        return r;
    }
  if (w.tasks.shop_orders && w.tasks.shop_orders->estate_cancellations)
    for (const auto &row :
         w.tasks.shop_orders->estate_cancellations->cancellations.rows()) {
      auto payload = encode_task_shop_order_estate_fact(
          *w.tasks.shop_orders->estate_cancellations, row.first);
      if (!payload.ok)
        return {false, {}, payload.error};
      auto r = fact(row.first, "task_shop_order_estate_cancelled",
                    "task_shop_order_estate_cancellations");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.common.state().facts.rows())
    if (row.second.domain == private_shop_daily_rent_module) {
      if (!w.private_shop_daily_rent_enabled())
        return common_failure("PrivateShopDailyRentProfileRequired", row.first);
      auto payload = encode_private_shop_daily_rent_fact_payload(
          w.common, w.private_shop_daily_rent, row.second);
      if (!payload.ok)
        return {false, {}, payload.error};
      auto checked =
          fact(row.first, row.second.kind, private_shop_daily_rent_module);
      if (!checked.ok)
        return checked;
    }
  if (w.field_battle) {
    for (const auto &row : w.field_battle->contacts) {
      auto r = fact(row.first, "field_battle_contact", "military");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.field_battle->boundary_contacts) {
      auto r = fact(row.first, "field_battle_contact", "military");
      if (!r.ok)
        return r;
      if (row.second.reinforcement_queue) {
        const auto &queue = *row.second.reinforcement_queue;
        r = fact(queue.initialized_source.id, "field_battle_entry_queue", "military");
        if (!r.ok)
          return r;
        for (const auto &entry : queue.admitted) {
          r = fact(entry.application_source.id, "field_battle_entered", "military");
          if (!r.ok)
            return r;
        }
      } else if (row.second.entry_application_source) {
        r = fact(row.second.entry_application_source->id,
                 "field_battle_entered", "military");
        if (!r.ok)
          return r;
      }
    }
    for (const auto &row : w.field_battle->sessions) {
      auto r = fact(row.first, "field_battle_session", "military");
      if (!r.ok)
        return r;
      const auto &session = row.second;
      // NPC 开战可借 session 原头；仅真正独立 started 头才另登记。
      if (session.started_source &&
          session.started_source->kind == "field_battle_started") {
        r = fact(session.started_source->id, "field_battle_started", "military");
        if (!r.ok)
          return r;
      }
      for (const auto &segment : session.settlements) {
        r = fact(segment.fact_id, "field_battle_segment", "military");
        if (!r.ok)
          return r;
      }
      for (const auto &[id, formation] : session.formations)
        if (formation.ability)
        for (const auto &change : formation.ability->commander_changes) {
          r = fact(change.source_ref.id, "field_commander_changed", "military");
          if (!r.ok)
            return r;
        }
      for (const auto &[id, stratagem] : session.stratagems)
        if (stratagem.kind == FieldStratagemKind::Confuse) {
          r = fact(stratagem.adopted_source.id, "field_battle_stratagem", "military");
          if (!r.ok)
            return r;
        }
    }
    for (const auto &row : w.field_battle->exits) {
      auto r = fact(row.first, "field_battle_exit", "military");
      if (!r.ok)
        return r;
    }
    for (const auto &row : w.field_battle->council_facts) {
      auto r = fact(row.first, "military_council", "military");
      if (!r.ok)
        return r;
    }
    // Each new header has exactly one owned plan/step/ready payload. The
    // selected Field source validator separately authenticates the full chain;
    // this coverage list never licenses an orphan Common fact.
    for (const auto &row : w.field_battle->army_exit_plans) {
      auto r = fact(row.first, "field_army_exit_choice", "military");
      if (!r.ok)
        return r;
      for (const auto &step : row.second.steps) {
        r = fact(step.fact_id, "field_army_exit_step", "military");
        if (!r.ok)
          return r;
      }
      if (row.second.ready_source) {
        r = fact(row.second.ready_source->id, "field_army_exit_ready", "military");
        if (!r.ok)
          return r;
      }
    }
  }
  auto condition_headers = validate_native_condition_application_headers(
      w.common, *w.catalog,
      w.actor_foundation ? &*w.actor_foundation : nullptr);
  if (!condition_headers.ok)
    return condition_headers;
  for (const auto &row : w.common.state().facts.rows())
    if (owns_condition_application_fact_kind(row.second.kind)) {
      auto accepted = fact(row.first, row.second.kind, row.second.domain);
      if (!accepted.ok)
        return accepted;
    }
  if (facts.size() != w.common.state().facts.size()) {
    // 失败仍保原拒绝；指出首个未被真 payload 覆盖的头，不以总数吞掉原来源。
    for (const auto &row : w.common.state().facts.rows())
      if (!facts.contains(row.first)) {
        auto missing = common_failure("UnsupportedNativeFactPayload",
                                      row.second.domain + "/" +
                                          row.second.kind + "/" + row.first,
                                      ErrorKind::unsupported_capability);
        // 原拒绝与 path 不变；显式元数据可穿过命令边界，不吞掉真正缺项。
        missing.error.phase = "payload_registry";
        missing.error.domain = row.second.domain;
        missing.error.command = row.second.kind;
        missing.error.source_id = row.first;
        return missing;
      }
    return common_failure("UnsupportedNativeFactPayload", {},
                          ErrorKind::unsupported_capability);
  }
  auto action = [&](const StableId &id, const StableId &kind) -> Status {
    auto p = w.common.action(id);
    if (!p || p->id != id || p->kind != kind || !actions.insert(id).second)
      return common_failure("ResidentActionPayloadMismatch", id);
    return common_success();
  };
  for (const auto &row : w.travel.actions.rows()) {
    auto r = action(row.first, "travel");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.travel.road_actions.rows()) {
    auto r = action(row.first, "travel");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.travel.rescues.rows()) {
    auto r = action(row.first, "rescue");
    if (!r.ok)
      return r;
  }
  // 行动仅在真实 started 阶段拥有 Common action；完成历史只保事实正文。
  if (w.military_action_time)
    for (const auto &row : w.military_action_time->dynamic_action_facts.rows())
      if (row.second.phase == "started") {
        auto r = action(row.first, row.second.command);
        if (!r.ok)
          return r;
      }
  for (const auto &row : w.military.actions) {
    auto r =
        action(row.action_id, std::holds_alternative<OfficeAccept>(row.request)
                                  ? "office_accept"
                                  : "army_form");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.military.logistics_actions) {
    if (row.request.kind != "army_return" &&
        row.request.kind != "army_resupply")
      return common_failure("ResidentActionPayloadMismatch", row.action_id);
    auto r = action(row.action_id, row.request.kind);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.naval.repairs.starts) {
    bool closed = false;
    for (const auto &result : w.naval.repairs.results)
      if (result.action_id == row.id)
        closed = true;
    if (!closed) {
      auto r = action(row.id, "naval_repair");
      if (!r.ok)
        return r;
    }
  }
  for (const auto &row : w.military.training_actions) {
    auto r = action(row.action_id, "military_train");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.common.state().actions.rows())
    if (row.second.kind == "wait") {
      if (row.second.remaining_ticks != 1)
        return common_failure("InvalidWaitAction");
      actions.insert(row.first);
    }
  for (const auto &row : w.tasks.actions.rows()) {
    auto p = w.common.action(row.first);
    if (!p || (p->kind != "accept_offer" && p->kind != "report_offer" &&
               p->kind != "deliver_message"))
      return common_failure("ResidentActionPayloadMismatch", row.first);
    auto r = action(row.first, p->kind);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.estate.actions.rows()) {
    auto r = action(row.first, "estate_build");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.common.state().actions.rows())
    if (row.second.kind == "tactic_lesson") {
      if (!w.growth_tactic_lessons_enabled())
        return common_failure("GrowthTacticLessonProfileRequired");
      std::size_t n = 0;
      for (const auto &start : w.growth_tactic_lessons.starts.rows())
        if (start.second.action_id == row.first)
          ++n;
      if (n != 1)
        return common_failure("TacticLessonActualStartRequired", row.first);
      auto r = action(row.first, "tactic_lesson");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.common.state().actions.rows())
    if (row.second.kind == "family_child_care_relief") {
      if (!w.family_care_relief_enabled() ||
          !w.child_care_relief.starts.find(row.first))
        return common_failure("ChildCareReliefStartRequired", row.first);
      auto r = action(row.first, "family_child_care_relief");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.common.state().actions.rows())
    if (row.second.kind == "family_child_care") {
      if (!w.family_birth_enabled() ||
          !w.family_birth.care_starts.find(row.first))
        return common_failure("ChildCareStartRequired", row.first);
      auto r = action(row.first, "family_child_care");
      if (!r.ok)
        return r;
    }
  for (const auto &row : w.family.actions.rows()) {
    auto r = action(row.first, "family_adopt");
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.growth.actions.rows()) {
    auto p = w.common.action(row.first);
    if (!p || !growth_action_kind(p->kind))
      return common_failure("ResidentActionPayloadMismatch", row.first);
    auto r = action(row.first, p->kind);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.population_military.actions.rows()) {
    if (!w.population_military_enabled() ||
        !population_military_command_kind(row.second.kind))
      return common_failure("ResidentActionPayloadMismatch", row.first);
    auto r = action(row.first, row.second.kind);
    if (!r.ok)
      return r;
  }
  for (const auto &row : governance_payloads.actions) {
    auto r = action(row.action_id, "governance_appease");
    if (!r.ok)
      return r;
  }
  for (const auto &row : envoy_payloads.actions) {
    auto r = action(row.action_id, row.kind);
    if (!r.ok)
      return r;
  }
  for (const auto &row : w.private_shops.operations.rows())
    if (row.second.result_fact_id.empty()) {
      auto r = action(row.first, "private_shop_operate");
      if (!r.ok)
        return r;
      auto payload = encode_private_shop_action_payload(
          w.common, w.private_shops, *w.common.action(row.first));
      if (!payload.ok)
        return {false, {}, payload.error};
    }
  if (w.tasks.shop_orders)
    for (const auto &row : w.tasks.shop_orders->actions.rows()) {
      auto r = action(row.first, "task_shop_order_deliver");
      if (!r.ok)
        return r;
    }
  {
    auto payloads = personal_appointment_action_payloads(
        w.common, w.interaction.personal_appointments
                      ? &*w.interaction.personal_appointments
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "personal_appointment_actions")) {
      auto r = action(row.first, personal_appointment_action_kind);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = visit_appointment_action_payloads(
        w.common, w.interaction.visit_appointment_meetings
                      ? &*w.interaction.visit_appointment_meetings
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "visit_appointment_actions")) {
      auto r = action(row.first, visit_appointment_meeting_action_kind);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = contact_inquiry_action_payloads(
        w.common, w.interaction.contact_inquiries
                      ? &*w.interaction.contact_inquiries
                      : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "contact_inquiry_actions")) {
      auto r = action(row.first, contact_inquiry_action_kind);
      if (!r.ok)
        return r;
    }
  }
  {
    auto payloads = task_message_return_action_payloads(
        w.common,
        w.tasks.message_returns ? &*w.tasks.message_returns : nullptr);
    if (!payloads.ok)
      return {false, {}, payloads.error};
    for (const auto &row :
         boundary::object(payloads.value, "task_message_return_actions")) {
      auto r = action(row.first, task_message_return_collect_command);
      if (!r.ok)
        return r;
    }
  }
  if (actions.size() != w.common.state().actions.size())
    return common_failure("UnsupportedNativeActionPayload", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace
// 只读取已经完成验证的原伤亡账，不推演战果。
FamilyDeathOriginRead WorldBundle::family_death_read() const {
  return
      [this](
          const StableId &actor) -> Outcome<std::optional<FamilyDeathOrigin>> {
        auto source = query_military_actor_death(common, military, actor);
        if (!source.ok)
          return {false, {}, source.error};
        if (!source.value)
          return {true, std::nullopt, {}};
        const auto &d = *source.value;
        return {true,
                FamilyDeathOrigin{d.actor_id, d.casualty_fact_id, d.battle_id,
                                  d.tick},
                {}};
      };
}
// 军事来源只读同一Common候选；不回调旧Family/legacy副本，也不把未来事实当当前资格。
FamilyLegacyPlayableRead WorldBundle::family_legacy_playable_read() const {
  return [this](const StableId &actor, Integer tick,
                const StableId &boundary) -> Status {
    if (boundary.empty() && tick != common.tick())
      return common_failure("MilitaryOccupationBoundaryRequired");
    auto source =
        boundary.empty()
            ? query_military_actor_occupation(
                  common, military, naval_enabled() ? &naval : nullptr, actor)
            : query_military_actor_occupation_before(
                  common, military, naval_enabled() ? &naval : nullptr, actor,
                  tick, boundary);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &value = source.value;
    if (value.actor_id != actor || value.observed_tick != tick ||
        value.boundary_fact_id != boundary)
      return common_failure("MilitaryOccupationBoundaryMismatch", actor);
    if (!value.history_complete || !value.incomplete_source_ids.empty())
      return common_failure("MilitaryOccupationHistoryIncomplete", actor);
    if (!value.custody_source_ids.empty() || !value.battle_source_ids.empty() ||
        !value.siege_source_ids.empty())
      return common_failure("LegacySuccessorOccupied", actor);
    return common_success();
  };
}
// 遗赠只追加本域事实，历史玩家仍由既有合并控制链提供。
FamilyBequestPlayerRead WorldBundle::family_bequest_player_read() const {
  return [this](Integer tick, const StableId &fact) {
    return family_legacy_enabled()
               ? family_player_before_fact_with_legacy(
                     common, family, family_legacy, tick, fact)
               : family_player_before_fact(common, family, tick, fact);
  };
}
// 本人原信只经正式S3读口；责任预览与冷史各消费原领域配对接口。
FamilyInheritanceAccess WorldBundle::family_inheritance_access() const {
  TaskDeliveredDeathNoticeRead notice = [this](const StableId &observer,
                                               const StableId &binding)
      -> Outcome<std::optional<DeliveredTaskDeathNotice>> {
    auto source = make_task_message_notice_services(*this);
    if (!source.ok)
      return {false, {}, source.error};
    if (!source.value)
      return {
          false, {}, common_failure("TaskMessageNoticeProfileRequired").error};
    return read_delivered_task_death_notice(
        common, tasks, *catalog->tasks, *catalog->common,
        *catalog->task_message_notices, *tasks.message_notices,
        *tasks.message_handoffs, source.value->validate, observer, binding);
  };
  auto estate_read = services.estate_access();
  EconomicPrivateHistoryContext context{
      common,
      *catalog->common,
      estate,
      *catalog->estate,
      dynamic_enabled() ? &local_economy : nullptr,
      dynamic_enabled() ? catalog->local_economy.get() : nullptr,
      fiscal_settlement_enabled() ? catalog->fiscal_settlement.get() : nullptr,
      fiscal_settlement_enabled() ? &fiscal_settlement : nullptr,
      occupation_fiscal_enabled() ? &occupation_fiscal_settlement : nullptr,
      public_relief_enabled() ? &public_relief : nullptr,
      &estate_read,
      estate_employment_enabled() ? &estate_employment : nullptr,
      private_shops_enabled() ? &private_shops : nullptr};
  auto original = family_inheritance_private_clearance_read(context, tasks,
                                                            *catalog->tasks);
  return {
      family_bequest_player_read(), family_received_death_notice_read(notice),
      [this, original](const StableId &actor, const StableId &wallet,
                       const StableId &bag, Integer tick,
                       const StableId &fact) {
        if (family_birth_enabled()) {
          auto care = fact.empty()
                          ? read_birth_care_responsibilities(
                                common, family_birth, actor)
                          : read_birth_care_responsibilities_before_inheritance(
                                common, family_birth, actor, tick, fact);
          if (!care.ok)
            return Status{false, {}, care.error};
          if (!care.value.pending_acceptance_ids.empty())
            return common_failure("InheritanceBirthCareResponsibilityPending",
                                  care.value.pending_acceptance_ids.front());
        }
        if (family_care_relief_enabled()) {
          auto care =
              fact.empty()
                  ? read_child_care_relief_responsibilities(
                        common, child_care_relief, actor)
                  : read_child_care_relief_responsibilities_before_inheritance(
                        common, child_care_relief, actor, tick, fact);
          if (!care.ok)
            return Status{false, {}, care.error};
          if (!care.value.pending_acceptance_ids.empty())
            return common_failure(
                "InheritanceChildCareReliefResponsibilityPending",
                care.value.pending_acceptance_ids.front());
        }
        if (growth_tactic_lessons_enabled()) {
          auto lesson =
              fact.empty()
                  ? read_growth_tactic_lesson_responsibilities(
                        common, growth_tactic_lessons, actor)
                  : read_growth_tactic_lesson_responsibilities_before_inheritance(
                        common, growth_tactic_lessons, actor, tick, fact);
          if (!lesson.ok)
            return Status{false, {}, lesson.error};
          if (!lesson.value.pending_source_ids.empty())
            return common_failure(
                "InheritanceTacticLessonResponsibilityPending",
                lesson.value.pending_source_ids.front());
        }
        return original(actor, wallet, bag, tick, fact);
      }};
}
// 此两口只读取原控制历史和Military/Naval占用，不读取或递归校验婚姻状态。
MarriageAccess WorldBundle::family_marriage_access() const {
  MarriageAccess access{
      family_bequest_player_read(), family_legacy_playable_read(), {}};
  if (family_birth_enabled())
    access.known_kinship = [this](const std::map<StableId, StableId> &parents,
                                  const StableId &first,
                                  const StableId &second) {
      return validate_birth_known_family_pair(
          common, *catalog->common, *catalog->family_lineage, family_birth,
          parents, first, second);
    };
  return access;
}
// 原拥有者在委托构造全程持费；所有可抛成员完成后才noexcept移交同一State。
WorldBundle::WorldBundle(std::shared_ptr<const PreparedCatalog> prepared,
                         JsonDecodedWorldSeedOwner &&owner)
    : WorldBundle(owner.require_catalog(std::move(prepared)),
                  owner.consume_seed()) {
  checkpoint_json_preimage = owner.release_owner();
}
WorldBundle::WorldBundle(std::shared_ptr<const PreparedCatalog> prepared,
                         InitialWorldSeed &&seed)
    : checkpoint_copy_memory(reserve_initial_world_seed_copy(seed)),
      catalog(std::move(prepared)),
      political_relations(
          seed.political_relations
              ? std::optional<
                    PoliticalRelationsState>{*seed.political_relations}
              : std::nullopt),
      political_passage(seed.political_passage ? std::optional<PoliticalPassageDecisionState>{*seed.political_passage} : std::nullopt),
      political_safe_exit(seed.political_safe_exit ? std::optional<SafeExitState>{*seed.political_safe_exit} : std::nullopt),
      common(std::move(seed.common), catalog->common),
      actor_foundation(
          seed.actor_foundation
              ? std::optional<
                    AuthoredActorFoundationState>{*seed.actor_foundation}
              : std::nullopt),
      character_injury_sources(
          seed.character_injury_sources
              ? std::optional<
                    CharacterInjurySourcesStateV1>{*seed.character_injury_sources}
              : std::nullopt),
      growth_permanent(
          seed.growth_permanent
              ? std::optional<GrowthPermanentState>{*seed.growth_permanent}
              : std::nullopt),
      life_age_checks(seed.life_age_checks
                          ? std::optional<LifeAgeState>{*seed.life_age_checks}
                          : std::nullopt),
      control_continuation(
          seed.control_continuation
              ? std::optional<
                    ControlContinuationState>{*seed.control_continuation}
              : std::nullopt),
      production_cycles(
          seed.production_cycles
              ? std::optional<EconomicWorkSourcesState>{*seed.production_cycles}
              : std::nullopt),
      life_age_checks_seed_complete(bool(seed.life_age_checks)),
      social_period_sources(
          seed.social_period_sources
              ? std::optional<SocialPeriodSources>{*seed.social_period_sources}
              : std::nullopt),
      world_timeline(
          seed.world_timeline
              ? std::optional<WorldTimelineState>{*seed.world_timeline}
              : std::nullopt),
      action_elapsed(
          seed.action_elapsed
              ? std::optional<ActionElapsedStore>{*seed.action_elapsed}
              : std::nullopt),
      facility_claims(
          seed.facility_claims
              ? std::optional<FacilityClaimsState>{*seed.facility_claims}
              : std::nullopt),
      travel(*seed.travel),
      movement_exact(
          seed.movement_exact
              ? std::optional<MovementExactState>{*seed.movement_exact}
              : std::nullopt),
      spatial_carriers(
          seed.spatial_carriers
              ? std::optional<SpatialCarrierState>{*seed.spatial_carriers}
              : std::nullopt),
      group_movement_executions(
          seed.group_movement_executions
              ? std::optional<
                    GroupMovementExecutionState>{*seed.group_movement_executions}
              : std::nullopt),
      transport_registrations(
          seed.transport_registrations
              ? std::optional<
                    TransportRegistrationState>{*seed.transport_registrations}
              : std::nullopt),
      economic_transport_expenses(
          seed.economic_transport_expenses
              ? std::optional<
                    EconomicTransportExpenseState>{*seed.economic_transport_expenses}
              : std::nullopt),
      private_transport_funding(
          seed.private_transport_funding
              ? std::optional<
                    PrivateTransportFundingState>{*seed.private_transport_funding}
              : std::nullopt),
      military(*seed.military),
      military_exact(
          seed.military_exact
              ? std::optional<MilitaryExactState>{*seed.military_exact}
              : std::nullopt),
      military_action_time(
          seed.military_action_time
              ? std::optional<
                    MilitaryActionTimeState>{*seed.military_action_time}
              : std::nullopt),
      field_battle(
          seed.field_battle
              ? std::optional<FieldBattleWorldState>{*seed.field_battle}
              : std::nullopt),
      public_supply_inspection(
          seed.public_supply_inspection
              ? std::optional<
                    PublicSupplyInspectionState>{*seed.public_supply_inspection}
              : std::nullopt),
      item_instances(
          seed.item_instances
              ? std::optional<ItemInstancesState>{*seed.item_instances}
              : std::nullopt),
      lot_properties(
          seed.lot_properties
              ? std::optional<LotPropertiesState>{*seed.lot_properties}
              : std::nullopt),
      craft_jobs(seed.craft_jobs
                     ? std::optional<CraftJobsState>{*seed.craft_jobs}
                     : std::nullopt),
      economic_facility_capacity_sources(
          seed.economic_facility_capacity_sources
              ? std::optional<
                    EconomicFacilitySourcesState>{*seed.economic_facility_capacity_sources}
              : std::nullopt),
      production_facility_conditions(
          seed.production_facility_conditions
              ? std::optional<
                    ProductionFacilityConditionsState>{*seed.production_facility_conditions}
              : std::nullopt),
      governance_work_expense(
          seed.governance_work_expense
              ? std::optional<
                    GovernanceWorkExpenseState>{*seed.governance_work_expense}
              : std::nullopt),
      political_budgets(
          seed.political_budgets
              ? std::optional<PoliticalBudgetState>{*seed.political_budgets}
              : std::nullopt),
      political_treaty_terms(
          seed.political_treaty_terms
              ? std::optional<
                    PoliticalTreatyTermsState>{*seed.political_treaty_terms}
              : std::nullopt),
      budget_execution(
          seed.budget_execution
              ? std::optional<BudgetExecutionState>{*seed.budget_execution}
              : std::nullopt),
      population_transport(
          seed.population_transport
              ? std::optional<
                    PopulationTransportState>{*seed.population_transport}
              : std::nullopt),
      household_purchases(
          seed.household_purchases
              ? std::optional<HouseholdPurchaseState>{*seed.household_purchases}
              : std::nullopt),
      household_retail_settlements(
          seed.household_retail_settlements
              ? std::optional<
                    HouseholdRetailState>{*seed.household_retail_settlements}
              : std::nullopt),
      military_tactic_learning(seed.military_tactic_learning
                                   ? *seed.military_tactic_learning
                                   : MilitaryTacticLearningState{}),
      military_tactic_learning_seed_complete(
          bool(seed.military_tactic_learning)),
      naval(seed.naval ? *seed.naval : NavalState{}),
      naval_seed_complete(bool(seed.naval)), interaction(*seed.interaction),
      social(*seed.social), estate(*seed.estate), tasks(*seed.tasks),
      envoy_tasks(seed.envoy_tasks ? *seed.envoy_tasks : EnvoyTaskState{}),
      envoy_tasks_seed_complete(bool(seed.envoy_tasks)),
      governance_tasks(seed.governance_tasks ? *seed.governance_tasks
                                             : GovernanceTaskState{}),
      governance_tasks_seed_complete(bool(seed.governance_tasks)),
      family(*seed.family),
      family_life(seed.family_life ? *seed.family_life : FamilyDeathState{}),
      family_life_seed_complete(bool(seed.family_life)),
      family_legacy(seed.family_legacy ? *seed.family_legacy
                                       : FamilyLegacyState{}),
      family_legacy_seed_complete(bool(seed.family_legacy)),
      family_bequest(seed.family_bequest ? *seed.family_bequest
                                         : FamilyBequestState{}),
      family_bequest_seed_complete(bool(seed.family_bequest)),
      family_inheritance(seed.family_inheritance ? *seed.family_inheritance
                                                 : FamilyInheritanceState{}),
      family_inheritance_seed_complete(bool(seed.family_inheritance)),
      family_voluntary_paths(seed.family_voluntary_paths
                                 ? *seed.family_voluntary_paths
                                 : VoluntaryPathState{}),
      family_voluntary_paths_seed_complete(bool(seed.family_voluntary_paths)),
      family_collateral_legacy(seed.family_collateral_legacy
                                   ? *seed.family_collateral_legacy
                                   : CollateralControlState{}),
      family_collateral_legacy_seed_complete(
          bool(seed.family_collateral_legacy)),
      family_biological_legacy(seed.family_biological_legacy
                                   ? *seed.family_biological_legacy
                                   : BiologicalControlState{}),
      family_biological_legacy_seed_complete(
          bool(seed.family_biological_legacy)),
      family_disciple_legacy(seed.family_disciple_legacy
                                 ? *seed.family_disciple_legacy
                                 : DiscipleControlState{}),
      family_disciple_legacy_seed_complete(bool(seed.family_disciple_legacy)),
      family_apprenticeship(seed.family_apprenticeship
                                ? *seed.family_apprenticeship
                                : FamilyApprenticeshipState{}),
      family_apprenticeship_seed_complete(bool(seed.family_apprenticeship)),
      family_marriage(seed.family_marriage ? *seed.family_marriage
                                           : FamilyMarriageState{}),
      family_marriage_seed_complete(bool(seed.family_marriage)),
      family_birth(seed.family_birth ? *seed.family_birth : FamilyBirthState{}),
      family_birth_seed_complete(bool(seed.family_birth)),
      child_care_relief(seed.child_care_relief ? *seed.child_care_relief
                                               : ChildCareReliefState{}),
      child_care_relief_seed_complete(bool(seed.child_care_relief)),
      estate_employment(seed.estate_employment ? *seed.estate_employment
                                               : EstateEmploymentState{}),
      estate_employment_seed_complete(bool(seed.estate_employment)),
      economic_supply_policies(seed.economic_supply_policies
                                   ? *seed.economic_supply_policies
                                   : EconomicSupplyPolicyState{}),
      economic_supply_policies_seed_complete(
          bool(seed.economic_supply_policies)),
      political_supply_adoptions(seed.political_supply_adoptions
                                     ? *seed.political_supply_adoptions
                                     : PoliticalSupplyAdoptionState{}),
      political_supply_adoptions_seed_complete(
          bool(seed.political_supply_adoptions)),
      personal(*seed.personal), growth(*seed.growth),
      card_acquisitions(seed.card_acquisitions ? *seed.card_acquisitions
                                               : CardAcquisitionState{}),
      card_acquisitions_seed_complete(bool(seed.card_acquisitions)),
      profile_deliveries(seed.profile_deliveries ? *seed.profile_deliveries
                                                 : ProfileDeliveryOutbox{}),
      growth_tactic_lessons(seed.growth_tactic_lessons
                                ? *seed.growth_tactic_lessons
                                : GrowthTacticLessonState{}),
      growth_tactic_lessons_seed_complete(bool(seed.growth_tactic_lessons)),
      personal_recovery(seed.personal_recovery ? *seed.personal_recovery
                                               : GrowthRecoveryState{}),
      personal_recovery_seed_complete(bool(seed.personal_recovery)),
      growth_market_history(seed.growth_market_history
                                ? *seed.growth_market_history
                                : GrowthMarketHistoryState{}),
      growth_market_history_seed_complete(bool(seed.growth_market_history)),
      private_shops(seed.private_shops ? *seed.private_shops
                                       : PrivateShopState{}),
      private_shops_seed_complete(bool(seed.private_shops)),
      private_shop_operation_sources(
          seed.private_shop_operation_sources
              ? std::optional<
                    PrivateShopOperationSourcesState>{*seed.private_shop_operation_sources}
              : std::nullopt),
      private_shop_operation_capabilities(
          seed.private_shop_operation_capabilities
              ? std::optional<
                    PrivateShopOperationCapabilitiesState>{*seed.private_shop_operation_capabilities}
              : std::nullopt),
      private_shop_household_custody(
          seed.private_shop_household_custody
              ? std::optional<
                    PrivateShopHouseholdCustodyState>{*seed.private_shop_household_custody}
              : std::nullopt),
      private_shop_daily_rent(seed.private_shop_daily_rent
                                  ? *seed.private_shop_daily_rent
                                  : PrivateShopDailyRentState{}),
      private_shop_daily_rent_seed_complete(bool(seed.private_shop_daily_rent)),
      private_shop_command_results(seed.private_shop_command_results
                                       ? *seed.private_shop_command_results
                                       : PrivateShopCommandResultsState{}),
      private_shop_command_results_seed_complete(
          bool(seed.private_shop_command_results)),
      private_shop_liquidations(seed.private_shop_liquidations
                                    ? *seed.private_shop_liquidations
                                    : PrivateShopLiquidationState{}),
      private_shop_liquidations_seed_complete(
          bool(seed.private_shop_liquidations)),
      political_shop_licensing(seed.political_shop_licensing
                                   ? *seed.political_shop_licensing
                                   : PoliticalShopLicensingState{}),
      political_shop_licensing_seed_complete(
          bool(seed.political_shop_licensing)),
      political_shop_licensing_receipts(
          seed.political_shop_licensing_receipts
              ? std::optional<
                    PoliticalShopLicensingReceiptState>{*seed.political_shop_licensing_receipts}
              : std::nullopt),
      political_governance_continuations(
          seed.political_governance_continuations
              ? std::optional<
                    PoliticalGovernanceContinuationState>{*seed.political_governance_continuations}
              : std::nullopt),
      governance_invitation_replies(
          seed.governance_invitation_replies
              ? std::optional<
                    GovernanceInvitationReplyState>{*seed.governance_invitation_replies}
              : std::nullopt),
      political_head_handovers(
          seed.political_head_handovers
              ? std::optional<
                    PoliticalHeadHandoverState>{*seed.political_head_handovers}
              : std::nullopt),
      political_successor_rounds(
          seed.political_successor_rounds
              ? std::optional<
                    PoliticalSuccessorRoundState>{*seed.political_successor_rounds}
              : std::nullopt),
      political_term_handovers(
          seed.political_term_handovers
              ? std::optional<
                    PoliticalTermHandoverState>{*seed.political_term_handovers}
              : std::nullopt),
      political_office_receptions(
          seed.political_office_receptions
              ? std::optional<
                    PoliticalOfficeReceptionState>{*seed.political_office_receptions}
              : std::nullopt),
      political_office_resignations(
          seed.political_office_resignations
              ? std::optional<
                    PoliticalOfficeResignationState>{*seed.political_office_resignations}
              : std::nullopt),
      political_subordinate_appointments(
          seed.political_subordinate_appointments
              ? std::optional<
                    PoliticalSubordinateAppointmentState>{*seed.political_subordinate_appointments}
              : std::nullopt),
      political_dismissal_sources(
          seed.political_dismissal_sources
              ? std::optional<
                    PoliticalDismissalSourceState>{*seed.political_dismissal_sources}
              : std::nullopt),
      political_governance_agencies(
          seed.political_governance_agencies
              ? std::optional<
                    PoliticalGovernanceAgencyState>{*seed.political_governance_agencies}
              : std::nullopt),
      political_successions(
          seed.political_successions
              ? std::optional<
                    PoliticalSuccessionState>{*seed.political_successions}
              : std::nullopt),
      political_shop_registry_successions(
          seed.political_shop_registry_successions
              ? *seed.political_shop_registry_successions
              : PoliticalShopRegistrySuccessionState{}),
      political_shop_registry_successions_seed_complete(
          bool(seed.political_shop_registry_successions)),
      local_economy(seed.local_economy ? *seed.local_economy
                                       : LocalEconomyState{}),
      economic_obligation_sources(
          seed.economic_obligation_sources
              ? std::optional<
                    EconomicObligationSourcesState>{*seed.economic_obligation_sources}
              : std::nullopt),
      organization(seed.organization ? *seed.organization
                                     : OrganizationState{}),
      organization_runtime(
          seed.organization_runtime
              ? std::optional<OrganizationRuntime>{*seed.organization_runtime}
              : std::nullopt),
      governance_programs(
          seed.governance_programs
              ? std::optional<GovernanceProgramState>{*seed.governance_programs}
              : std::nullopt),
      affairs(seed.affairs ? *seed.affairs : AffairsState{}),
      director(seed.director ? *seed.director : DirectorState{}),
      population_military(seed.population_military ? *seed.population_military
                                                   : PopulationMilitaryState{}),
      diplomacy(seed.diplomacy ? *seed.diplomacy : DiplomacyState{}),
      diplomacy_seed_complete(bool(seed.diplomacy)),
      treaty_passage(seed.treaty_passage ? *seed.treaty_passage
                                         : TreatyPassageState{}),
      treaty_passage_seed_complete(bool(seed.treaty_passage)),
      political_takeover(seed.political_takeover ? *seed.political_takeover
                                                 : PoliticalTakeoverState{}),
      political_takeover_seed_complete(bool(seed.political_takeover)),
      economic_takeover_execution(seed.economic_takeover_execution
                                      ? *seed.economic_takeover_execution
                                      : EconomicTakeoverExecutionState{}),
      economic_takeover_seed_complete(bool(seed.economic_takeover_execution)),
      politics(seed.politics ? *seed.politics : PoliticsState{}),
      political_fiscal(seed.political_fiscal ? *seed.political_fiscal
                                             : PoliticalFiscalState{}),
      political_military_appointments(
          seed.political_military_appointments
              ? *seed.political_military_appointments
              : PoliticalMilitaryAppointmentState{}),
      political_military_appointments_seed_complete(
          bool(seed.political_military_appointments)),
      political_field_appointments(seed.political_field_appointments
                                       ? *seed.political_field_appointments
                                       : PoliticalFieldAppointmentState{}),
      political_field_appointments_seed_complete(
          bool(seed.political_field_appointments)),
      politics_governance(seed.politics_governance ? *seed.politics_governance
                                                   : PoliticsGovernanceState{}),
      politics_governance_seed_complete(bool(seed.politics_governance)),
      fiscal_settlement(seed.fiscal_settlement ? *seed.fiscal_settlement
                                               : FiscalSettlementState{}),
      fiscal_settlement_seed_complete(bool(seed.fiscal_settlement)),
      fiscal_taxpayer_continuity(seed.fiscal_taxpayer_continuity
                                     ? *seed.fiscal_taxpayer_continuity
                                     : FiscalTaxpayerContinuityState{}),
      fiscal_taxpayer_continuity_seed_complete(
          bool(seed.fiscal_taxpayer_continuity)),
      household_provision_management(seed.household_provision_management
                                         ? *seed.household_provision_management
                                         : HouseholdProvisionManagementState{}),
      household_provision_management_seed_complete(
          bool(seed.household_provision_management)),
      production_disruptions(seed.production_disruptions
                                 ? *seed.production_disruptions
                                 : ProductionDisruptionState{}),
      production_disruptions_seed_complete(bool(seed.production_disruptions)),
      public_relief(seed.public_relief ? *seed.public_relief
                                       : PublicReliefState{}),
      public_relief_seed_complete(bool(seed.public_relief)),
      politics_seed_complete(bool(seed.politics)),
      political_fiscal_seed_complete(bool(seed.political_fiscal)),
      occupation_fiscal(seed.occupation_fiscal ? *seed.occupation_fiscal
                                               : OccupationFiscalState{}),
      occupation_fiscal_seed_complete(bool(seed.occupation_fiscal)),
      occupation_fiscal_settlement(seed.occupation_fiscal_settlement
                                       ? *seed.occupation_fiscal_settlement
                                       : OccupationFiscalSettlementState{}),
      occupation_fiscal_settlement_seed_complete(
          bool(seed.occupation_fiscal_settlement)),
      player_plan(seed.player_plan ? *seed.player_plan : PlayerPlanState{}),
      player_plan_seed_complete(bool(seed.player_plan)),
      population_seed_complete(bool(seed.population_military)),
      dynamic_seed_complete(seed.local_economy && seed.organization &&
                            seed.affairs && seed.director),
      services(*this) {
  common.bind_membership(&services, bool(catalog->field_battle));
  common.bind_task_owners(&services);
  common.bind_family_identity(&services);
  common.bind_household_owners(&services);
  common.bind_organization_owners(&services);
  common.bind_reservation_owners(&services);
  common.bind_protected_market_resources(services.market_history());
  observation_index = build_world_observation_index(common, social, tasks);
  map_presence_index = build_map_presence_index(common);
  economic_index = rebuild_economic_world_index(*this);
  fiscal_index = rebuild_economic_fiscal_index(*this);
  household_provision_index = rebuild_household_provision_index(*this);
  politics_view_index = make_politics_view_index(
      common, politics, organization,
      catalog->organization ? *catalog->organization : OrganizationCatalog{},
      social, &political_fiscal);
  governance_view_index = make_governance_view_index(
      common, politics_governance, tasks, governance_tasks);
  rebuild_diplomacy_index();
}
// 同一完整私有seed消费构造；旧外来shared seed始终保原不可变读取路径。
WorldBundle::WorldBundle(std::shared_ptr<const PreparedCatalog> prepared,
                         DecodedWorldSeedOwner &&seed)
    : catalog(seed.require_complete(std::move(prepared))),
      political_relations(
          seed.political_relations
              ? std::optional<PoliticalRelationsState>{std::move(
                    *seed.political_relations)}
              : std::nullopt),
      political_passage(seed.political_passage ? std::optional<PoliticalPassageDecisionState>{std::move(*seed.political_passage)} : std::nullopt),
      political_safe_exit(seed.political_safe_exit ? std::optional<SafeExitState>{std::move(*seed.political_safe_exit)} : std::nullopt),
      common(std::move(seed.common), catalog->common),
      actor_foundation(
          seed.actor_foundation
              ? std::optional<AuthoredActorFoundationState>{std::move(
                    *seed.actor_foundation)}
              : std::nullopt),
      character_injury_sources(
          seed.character_injury_sources
              ? std::optional<CharacterInjurySourcesStateV1>{std::move(
                    *seed.character_injury_sources)}
              : std::nullopt),
      growth_permanent(seed.growth_permanent
                           ? std::optional<GrowthPermanentState>{std::move(
                                 *seed.growth_permanent)}
                           : std::nullopt),
      life_age_checks(
          seed.life_age_checks
              ? std::optional<LifeAgeState>{std::move(*seed.life_age_checks)}
              : std::nullopt),
      control_continuation(
          seed.control_continuation
              ? std::optional<ControlContinuationState>{std::move(
                    *seed.control_continuation)}
              : std::nullopt),
      production_cycles(seed.production_cycles
                            ? std::optional<EconomicWorkSourcesState>{std::move(
                                  *seed.production_cycles)}
                            : std::nullopt),
      life_age_checks_seed_complete(bool(seed.life_age_checks)),
      social_period_sources(seed.social_period_sources
                                ? std::optional<SocialPeriodSources>{std::move(
                                      *seed.social_period_sources)}
                                : std::nullopt),
      world_timeline(seed.world_timeline
                         ? std::optional<WorldTimelineState>{std::move(
                               *seed.world_timeline)}
                         : std::nullopt),
      action_elapsed(seed.action_elapsed
                         ? std::optional<ActionElapsedStore>{std::move(
                               *seed.action_elapsed)}
                         : std::nullopt),
      facility_claims(seed.facility_claims
                          ? std::optional<FacilityClaimsState>{std::move(
                                *seed.facility_claims)}
                          : std::nullopt),
      travel(std::move(*seed.travel)),
      movement_exact(seed.movement_exact
                         ? std::optional<MovementExactState>{std::move(
                               *seed.movement_exact)}
                         : std::nullopt),
      spatial_carriers(seed.spatial_carriers
                           ? std::optional<SpatialCarrierState>{std::move(
                                 *seed.spatial_carriers)}
                           : std::nullopt),
      group_movement_executions(
          seed.group_movement_executions
              ? std::optional<GroupMovementExecutionState>{std::move(
                    *seed.group_movement_executions)}
              : std::nullopt),
      transport_registrations(
          seed.transport_registrations
              ? std::optional<TransportRegistrationState>{std::move(
                    *seed.transport_registrations)}
              : std::nullopt),
      economic_transport_expenses(
          seed.economic_transport_expenses
              ? std::optional<EconomicTransportExpenseState>{std::move(
                    *seed.economic_transport_expenses)}
              : std::nullopt),
      private_transport_funding(
          seed.private_transport_funding
              ? std::optional<PrivateTransportFundingState>{std::move(
                    *seed.private_transport_funding)}
              : std::nullopt),
      military(std::move(*seed.military)),
      military_exact(seed.military_exact
                         ? std::optional<MilitaryExactState>{std::move(
                               *seed.military_exact)}
                         : std::nullopt),
      military_action_time(
          seed.military_action_time
              ? std::optional<MilitaryActionTimeState>{std::move(
                    *seed.military_action_time)}
              : std::nullopt),
      field_battle(seed.field_battle
                       ? std::optional<FieldBattleWorldState>{std::move(
                             *seed.field_battle)}
                       : std::nullopt),
      public_supply_inspection(
          seed.public_supply_inspection
              ? std::optional<PublicSupplyInspectionState>{std::move(
                    *seed.public_supply_inspection)}
              : std::nullopt),
      item_instances(seed.item_instances
                         ? std::optional<ItemInstancesState>{std::move(
                               *seed.item_instances)}
                         : std::nullopt),
      lot_properties(seed.lot_properties
                         ? std::optional<LotPropertiesState>{std::move(
                               *seed.lot_properties)}
                         : std::nullopt),
      craft_jobs(seed.craft_jobs ? std::optional<CraftJobsState>{std::move(
                                       *seed.craft_jobs)}
                                 : std::nullopt),
      economic_facility_capacity_sources(
          seed.economic_facility_capacity_sources
              ? std::optional<EconomicFacilitySourcesState>{std::move(
                    *seed.economic_facility_capacity_sources)}
              : std::nullopt),
      production_facility_conditions(
          seed.production_facility_conditions
              ? std::optional<ProductionFacilityConditionsState>{std::move(
                    *seed.production_facility_conditions)}
              : std::nullopt),
      governance_work_expense(
          seed.governance_work_expense
              ? std::optional<GovernanceWorkExpenseState>{std::move(
                    *seed.governance_work_expense)}
              : std::nullopt),
      political_budgets(seed.political_budgets
                            ? std::optional<PoliticalBudgetState>{std::move(
                                  *seed.political_budgets)}
                            : std::nullopt),
      political_treaty_terms(
          seed.political_treaty_terms
              ? std::optional<PoliticalTreatyTermsState>{std::move(
                    *seed.political_treaty_terms)}
              : std::nullopt),
      budget_execution(seed.budget_execution
                           ? std::optional<BudgetExecutionState>{std::move(
                                 *seed.budget_execution)}
                           : std::nullopt),
      population_transport(
          seed.population_transport
              ? std::optional<PopulationTransportState>{std::move(
                    *seed.population_transport)}
              : std::nullopt),
      household_purchases(seed.household_purchases
                              ? std::optional<HouseholdPurchaseState>{std::move(
                                    *seed.household_purchases)}
                              : std::nullopt),
      household_retail_settlements(
          seed.household_retail_settlements
              ? std::optional<HouseholdRetailState>{std::move(
                    *seed.household_retail_settlements)}
              : std::nullopt),
      military_tactic_learning(seed.military_tactic_learning
                                   ? std::move(*seed.military_tactic_learning)
                                   : MilitaryTacticLearningState{}),
      military_tactic_learning_seed_complete(
          bool(seed.military_tactic_learning)),
      naval(seed.naval ? std::move(*seed.naval) : NavalState{}),
      naval_seed_complete(bool(seed.naval)),
      interaction(std::move(*seed.interaction)),
      social(std::move(*seed.social)), estate(std::move(*seed.estate)),
      tasks(std::move(*seed.tasks)),
      envoy_tasks(seed.envoy_tasks ? std::move(*seed.envoy_tasks)
                                   : EnvoyTaskState{}),
      envoy_tasks_seed_complete(bool(seed.envoy_tasks)),
      governance_tasks(seed.governance_tasks ? std::move(*seed.governance_tasks)
                                             : GovernanceTaskState{}),
      governance_tasks_seed_complete(bool(seed.governance_tasks)),
      family(std::move(*seed.family)),
      family_life(seed.family_life ? std::move(*seed.family_life)
                                   : FamilyDeathState{}),
      family_life_seed_complete(bool(seed.family_life)),
      family_legacy(seed.family_legacy ? std::move(*seed.family_legacy)
                                       : FamilyLegacyState{}),
      family_legacy_seed_complete(bool(seed.family_legacy)),
      family_bequest(seed.family_bequest ? std::move(*seed.family_bequest)
                                         : FamilyBequestState{}),
      family_bequest_seed_complete(bool(seed.family_bequest)),
      family_inheritance(seed.family_inheritance
                             ? std::move(*seed.family_inheritance)
                             : FamilyInheritanceState{}),
      family_inheritance_seed_complete(bool(seed.family_inheritance)),
      family_voluntary_paths(seed.family_voluntary_paths
                                 ? std::move(*seed.family_voluntary_paths)
                                 : VoluntaryPathState{}),
      family_voluntary_paths_seed_complete(bool(seed.family_voluntary_paths)),
      family_collateral_legacy(seed.family_collateral_legacy
                                   ? std::move(*seed.family_collateral_legacy)
                                   : CollateralControlState{}),
      family_collateral_legacy_seed_complete(
          bool(seed.family_collateral_legacy)),
      family_biological_legacy(seed.family_biological_legacy
                                   ? std::move(*seed.family_biological_legacy)
                                   : BiologicalControlState{}),
      family_biological_legacy_seed_complete(
          bool(seed.family_biological_legacy)),
      family_disciple_legacy(seed.family_disciple_legacy
                                 ? std::move(*seed.family_disciple_legacy)
                                 : DiscipleControlState{}),
      family_disciple_legacy_seed_complete(bool(seed.family_disciple_legacy)),
      family_apprenticeship(seed.family_apprenticeship
                                ? std::move(*seed.family_apprenticeship)
                                : FamilyApprenticeshipState{}),
      family_apprenticeship_seed_complete(bool(seed.family_apprenticeship)),
      family_marriage(seed.family_marriage ? std::move(*seed.family_marriage)
                                           : FamilyMarriageState{}),
      family_marriage_seed_complete(bool(seed.family_marriage)),
      family_birth(seed.family_birth ? std::move(*seed.family_birth)
                                     : FamilyBirthState{}),
      family_birth_seed_complete(bool(seed.family_birth)),
      child_care_relief(seed.child_care_relief
                            ? std::move(*seed.child_care_relief)
                            : ChildCareReliefState{}),
      child_care_relief_seed_complete(bool(seed.child_care_relief)),
      estate_employment(seed.estate_employment
                            ? std::move(*seed.estate_employment)
                            : EstateEmploymentState{}),
      estate_employment_seed_complete(bool(seed.estate_employment)),
      economic_supply_policies(seed.economic_supply_policies
                                   ? std::move(*seed.economic_supply_policies)
                                   : EconomicSupplyPolicyState{}),
      economic_supply_policies_seed_complete(
          bool(seed.economic_supply_policies)),
      political_supply_adoptions(
          seed.political_supply_adoptions
              ? std::move(*seed.political_supply_adoptions)
              : PoliticalSupplyAdoptionState{}),
      political_supply_adoptions_seed_complete(
          bool(seed.political_supply_adoptions)),
      personal(std::move(*seed.personal)), growth(std::move(*seed.growth)),
      card_acquisitions(seed.card_acquisitions
                            ? std::move(*seed.card_acquisitions)
                            : CardAcquisitionState{}),
      card_acquisitions_seed_complete(bool(seed.card_acquisitions)),
      profile_deliveries(seed.profile_deliveries
                             ? std::move(*seed.profile_deliveries)
                             : ProfileDeliveryOutbox{}),
      growth_tactic_lessons(seed.growth_tactic_lessons
                                ? std::move(*seed.growth_tactic_lessons)
                                : GrowthTacticLessonState{}),
      growth_tactic_lessons_seed_complete(bool(seed.growth_tactic_lessons)),
      personal_recovery(seed.personal_recovery
                            ? std::move(*seed.personal_recovery)
                            : GrowthRecoveryState{}),
      personal_recovery_seed_complete(bool(seed.personal_recovery)),
      growth_market_history(seed.growth_market_history
                                ? std::move(*seed.growth_market_history)
                                : GrowthMarketHistoryState{}),
      growth_market_history_seed_complete(bool(seed.growth_market_history)),
      private_shops(seed.private_shops ? std::move(*seed.private_shops)
                                       : PrivateShopState{}),
      private_shops_seed_complete(bool(seed.private_shops)),
      private_shop_operation_sources(
          seed.private_shop_operation_sources
              ? std::optional<PrivateShopOperationSourcesState>{std::move(
                    *seed.private_shop_operation_sources)}
              : std::nullopt),
      private_shop_operation_capabilities(
          seed.private_shop_operation_capabilities
              ? std::optional<PrivateShopOperationCapabilitiesState>{std::move(
                    *seed.private_shop_operation_capabilities)}
              : std::nullopt),
      private_shop_household_custody(
          seed.private_shop_household_custody
              ? std::optional<PrivateShopHouseholdCustodyState>{std::move(
                    *seed.private_shop_household_custody)}
              : std::nullopt),
      private_shop_daily_rent(seed.private_shop_daily_rent
                                  ? std::move(*seed.private_shop_daily_rent)
                                  : PrivateShopDailyRentState{}),
      private_shop_daily_rent_seed_complete(bool(seed.private_shop_daily_rent)),
      private_shop_command_results(
          seed.private_shop_command_results
              ? std::move(*seed.private_shop_command_results)
              : PrivateShopCommandResultsState{}),
      private_shop_command_results_seed_complete(
          bool(seed.private_shop_command_results)),
      private_shop_liquidations(seed.private_shop_liquidations
                                    ? std::move(*seed.private_shop_liquidations)
                                    : PrivateShopLiquidationState{}),
      private_shop_liquidations_seed_complete(
          bool(seed.private_shop_liquidations)),
      political_shop_licensing(seed.political_shop_licensing
                                   ? std::move(*seed.political_shop_licensing)
                                   : PoliticalShopLicensingState{}),
      political_shop_licensing_seed_complete(
          bool(seed.political_shop_licensing)),
      political_shop_licensing_receipts(
          seed.political_shop_licensing_receipts
              ? std::optional<PoliticalShopLicensingReceiptState>{std::move(
                    *seed.political_shop_licensing_receipts)}
              : std::nullopt),
      political_governance_continuations(
          seed.political_governance_continuations
              ? std::optional<PoliticalGovernanceContinuationState>{std::move(
                    *seed.political_governance_continuations)}
              : std::nullopt),
      governance_invitation_replies(
          seed.governance_invitation_replies
              ? std::optional<GovernanceInvitationReplyState>{std::move(
                    *seed.governance_invitation_replies)}
              : std::nullopt),
      political_head_handovers(
          seed.political_head_handovers
              ? std::optional<PoliticalHeadHandoverState>{std::move(
                    *seed.political_head_handovers)}
              : std::nullopt),
      political_successor_rounds(
          seed.political_successor_rounds
              ? std::optional<PoliticalSuccessorRoundState>{std::move(
                    *seed.political_successor_rounds)}
              : std::nullopt),
      political_term_handovers(
          seed.political_term_handovers
              ? std::optional<PoliticalTermHandoverState>{std::move(
                    *seed.political_term_handovers)}
              : std::nullopt),
      political_office_receptions(
          seed.political_office_receptions
              ? std::optional<PoliticalOfficeReceptionState>{std::move(
                    *seed.political_office_receptions)}
              : std::nullopt),
      political_office_resignations(
          seed.political_office_resignations
              ? std::optional<PoliticalOfficeResignationState>{std::move(
                    *seed.political_office_resignations)}
              : std::nullopt),
      political_subordinate_appointments(
          seed.political_subordinate_appointments
              ? std::optional<PoliticalSubordinateAppointmentState>{std::move(
                    *seed.political_subordinate_appointments)}
              : std::nullopt),
      political_dismissal_sources(
          seed.political_dismissal_sources
              ? std::optional<PoliticalDismissalSourceState>{std::move(
                    *seed.political_dismissal_sources)}
              : std::nullopt),
      political_governance_agencies(
          seed.political_governance_agencies
              ? std::optional<PoliticalGovernanceAgencyState>{std::move(
                    *seed.political_governance_agencies)}
              : std::nullopt),
      political_successions(
          seed.political_successions
              ? std::optional<PoliticalSuccessionState>{std::move(
                    *seed.political_successions)}
              : std::nullopt),
      political_shop_registry_successions(
          seed.political_shop_registry_successions
              ? std::move(*seed.political_shop_registry_successions)
              : PoliticalShopRegistrySuccessionState{}),
      political_shop_registry_successions_seed_complete(
          bool(seed.political_shop_registry_successions)),
      local_economy(seed.local_economy ? std::move(*seed.local_economy)
                                       : LocalEconomyState{}),
      economic_obligation_sources(
          seed.economic_obligation_sources
              ? std::optional<EconomicObligationSourcesState>{std::move(
                    *seed.economic_obligation_sources)}
              : std::nullopt),
      organization(seed.organization ? std::move(*seed.organization)
                                     : OrganizationState{}),
      organization_runtime(seed.organization_runtime
                               ? std::optional<OrganizationRuntime>{std::move(
                                     *seed.organization_runtime)}
                               : std::nullopt),
      governance_programs(seed.governance_programs
                              ? std::optional<GovernanceProgramState>{std::move(
                                    *seed.governance_programs)}
                              : std::nullopt),
      affairs(seed.affairs ? std::move(*seed.affairs) : AffairsState{}),
      director(seed.director ? std::move(*seed.director) : DirectorState{}),
      population_military(seed.population_military
                              ? std::move(*seed.population_military)
                              : PopulationMilitaryState{}),
      diplomacy(seed.diplomacy ? std::move(*seed.diplomacy) : DiplomacyState{}),
      diplomacy_seed_complete(bool(seed.diplomacy)),
      treaty_passage(seed.treaty_passage ? std::move(*seed.treaty_passage)
                                         : TreatyPassageState{}),
      treaty_passage_seed_complete(bool(seed.treaty_passage)),
      political_takeover(seed.political_takeover
                             ? std::move(*seed.political_takeover)
                             : PoliticalTakeoverState{}),
      political_takeover_seed_complete(bool(seed.political_takeover)),
      economic_takeover_execution(
          seed.economic_takeover_execution
              ? std::move(*seed.economic_takeover_execution)
              : EconomicTakeoverExecutionState{}),
      economic_takeover_seed_complete(bool(seed.economic_takeover_execution)),
      politics(seed.politics ? std::move(*seed.politics) : PoliticsState{}),
      political_fiscal(seed.political_fiscal ? std::move(*seed.political_fiscal)
                                             : PoliticalFiscalState{}),
      political_military_appointments(
          seed.political_military_appointments
              ? std::move(*seed.political_military_appointments)
              : PoliticalMilitaryAppointmentState{}),
      political_military_appointments_seed_complete(
          bool(seed.political_military_appointments)),
      political_field_appointments(
          seed.political_field_appointments
              ? std::move(*seed.political_field_appointments)
              : PoliticalFieldAppointmentState{}),
      political_field_appointments_seed_complete(
          bool(seed.political_field_appointments)),
      politics_governance(seed.politics_governance
                              ? std::move(*seed.politics_governance)
                              : PoliticsGovernanceState{}),
      politics_governance_seed_complete(bool(seed.politics_governance)),
      fiscal_settlement(seed.fiscal_settlement
                            ? std::move(*seed.fiscal_settlement)
                            : FiscalSettlementState{}),
      fiscal_settlement_seed_complete(bool(seed.fiscal_settlement)),
      fiscal_taxpayer_continuity(
          seed.fiscal_taxpayer_continuity
              ? std::move(*seed.fiscal_taxpayer_continuity)
              : FiscalTaxpayerContinuityState{}),
      fiscal_taxpayer_continuity_seed_complete(
          bool(seed.fiscal_taxpayer_continuity)),
      household_provision_management(
          seed.household_provision_management
              ? std::move(*seed.household_provision_management)
              : HouseholdProvisionManagementState{}),
      household_provision_management_seed_complete(
          bool(seed.household_provision_management)),
      production_disruptions(seed.production_disruptions
                                 ? std::move(*seed.production_disruptions)
                                 : ProductionDisruptionState{}),
      production_disruptions_seed_complete(bool(seed.production_disruptions)),
      public_relief(seed.public_relief ? std::move(*seed.public_relief)
                                       : PublicReliefState{}),
      public_relief_seed_complete(bool(seed.public_relief)),
      politics_seed_complete(bool(seed.politics)),
      political_fiscal_seed_complete(bool(seed.political_fiscal)),
      occupation_fiscal(seed.occupation_fiscal
                            ? std::move(*seed.occupation_fiscal)
                            : OccupationFiscalState{}),
      occupation_fiscal_seed_complete(bool(seed.occupation_fiscal)),
      occupation_fiscal_settlement(
          seed.occupation_fiscal_settlement
              ? std::move(*seed.occupation_fiscal_settlement)
              : OccupationFiscalSettlementState{}),
      occupation_fiscal_settlement_seed_complete(
          bool(seed.occupation_fiscal_settlement)),
      player_plan(seed.player_plan ? std::move(*seed.player_plan)
                                   : PlayerPlanState{}),
      player_plan_seed_complete(bool(seed.player_plan)),
      population_seed_complete(bool(seed.population_military)),
      dynamic_seed_complete(seed.local_economy && seed.organization &&
                            seed.affairs && seed.director),
      services(*this) {
  checkpoint_candidate_memory = std::move(seed.memory);
  common.bind_membership(&services, bool(catalog->field_battle));
  common.bind_task_owners(&services);
  common.bind_family_identity(&services);
  common.bind_household_owners(&services);
  common.bind_organization_owners(&services);
  common.bind_reservation_owners(&services);
  common.bind_protected_market_resources(services.market_history());
  observation_index = build_world_observation_index(common, social, tasks);
  map_presence_index = build_map_presence_index(common);
  economic_index = rebuild_economic_world_index(*this);
  fiscal_index = rebuild_economic_fiscal_index(*this);
  household_provision_index = rebuild_household_provision_index(*this);
  politics_view_index = make_politics_view_index(
      common, politics, organization,
      catalog->organization ? *catalog->organization : OrganizationCatalog{},
      social, &political_fiscal);
  governance_view_index = make_governance_view_index(
      common, politics_governance, tasks, governance_tasks);
  rebuild_diplomacy_index();
}
WorldBundle::WorldBundle(const WorldBundle &other)
    : checkpoint_copy_memory(reserve_world_checkpoint_copy(other)),
      catalog(other.catalog), political_relations(other.political_relations),
      political_passage(other.political_passage),
      political_safe_exit(other.political_safe_exit),
      common(other.common), actor_foundation(other.actor_foundation),
      character_injury_sources(other.character_injury_sources),
      growth_permanent(other.growth_permanent),
      life_age_checks(other.life_age_checks),
      control_continuation(other.control_continuation),
      production_cycles(other.production_cycles),
      life_age_checks_seed_complete(other.life_age_checks_seed_complete),
      world_timeline(other.world_timeline),
      social_period_sources(other.social_period_sources),
      action_elapsed(other.action_elapsed),
      facility_claims(other.facility_claims), travel(other.travel),
      movement_exact(other.movement_exact),
      spatial_carriers(other.spatial_carriers),
      group_movement_executions(other.group_movement_executions),
      transport_registrations(other.transport_registrations),
      economic_transport_expenses(other.economic_transport_expenses),
      private_transport_funding(other.private_transport_funding),
      military(other.military), military_exact(other.military_exact),
      military_action_time(other.military_action_time),
      field_battle(other.field_battle),
      public_supply_inspection(other.public_supply_inspection),
      item_instances(other.item_instances),
      lot_properties(other.lot_properties), craft_jobs(other.craft_jobs),
      economic_facility_capacity_sources(
          other.economic_facility_capacity_sources),
      production_facility_conditions(other.production_facility_conditions),
      governance_work_expense(other.governance_work_expense),
      political_budgets(other.political_budgets),
      political_treaty_terms(other.political_treaty_terms),
      budget_execution(other.budget_execution),
      population_transport(other.population_transport),
      household_purchases(other.household_purchases),
      household_retail_settlements(other.household_retail_settlements),
      military_tactic_learning(other.military_tactic_learning),
      military_tactic_learning_seed_complete(
          other.military_tactic_learning_seed_complete),
      naval(other.naval), naval_seed_complete(other.naval_seed_complete),
      interaction(other.interaction), social(other.social),
      estate(other.estate), tasks(other.tasks), envoy_tasks(other.envoy_tasks),
      envoy_tasks_seed_complete(other.envoy_tasks_seed_complete),
      governance_tasks(other.governance_tasks),
      governance_tasks_seed_complete(other.governance_tasks_seed_complete),
      family(other.family), family_life(other.family_life),
      family_life_seed_complete(other.family_life_seed_complete),
      family_legacy(other.family_legacy),
      family_legacy_seed_complete(other.family_legacy_seed_complete),
      family_bequest(other.family_bequest),
      family_bequest_seed_complete(other.family_bequest_seed_complete),
      family_inheritance(other.family_inheritance),
      family_inheritance_seed_complete(other.family_inheritance_seed_complete),
      family_voluntary_paths(other.family_voluntary_paths),
      family_voluntary_paths_seed_complete(
          other.family_voluntary_paths_seed_complete),
      family_collateral_legacy(other.family_collateral_legacy),
      family_collateral_legacy_seed_complete(
          other.family_collateral_legacy_seed_complete),
      family_biological_legacy(other.family_biological_legacy),
      family_biological_legacy_seed_complete(
          other.family_biological_legacy_seed_complete),
      family_disciple_legacy(other.family_disciple_legacy),
      family_disciple_legacy_seed_complete(
          other.family_disciple_legacy_seed_complete),
      family_apprenticeship(other.family_apprenticeship),
      family_apprenticeship_seed_complete(
          other.family_apprenticeship_seed_complete),
      family_marriage(other.family_marriage),
      family_marriage_seed_complete(other.family_marriage_seed_complete),
      family_birth(other.family_birth),
      family_birth_seed_complete(other.family_birth_seed_complete),
      child_care_relief(other.child_care_relief),
      child_care_relief_seed_complete(other.child_care_relief_seed_complete),
      estate_employment(other.estate_employment),
      estate_employment_seed_complete(other.estate_employment_seed_complete),
      economic_supply_policies(other.economic_supply_policies),
      economic_supply_policies_seed_complete(
          other.economic_supply_policies_seed_complete),
      political_supply_adoptions(other.political_supply_adoptions),
      political_supply_adoptions_seed_complete(
          other.political_supply_adoptions_seed_complete),
      personal(other.personal), growth(other.growth),
      card_acquisitions(other.card_acquisitions),
      card_acquisitions_seed_complete(other.card_acquisitions_seed_complete),
      profile_deliveries(other.profile_deliveries),
      growth_tactic_lessons(other.growth_tactic_lessons),
      growth_tactic_lessons_seed_complete(
          other.growth_tactic_lessons_seed_complete),
      personal_recovery(other.personal_recovery),
      personal_recovery_seed_complete(other.personal_recovery_seed_complete),
      growth_market_history(other.growth_market_history),
      growth_market_history_seed_complete(
          other.growth_market_history_seed_complete),
      private_shops(other.private_shops),
      private_shops_seed_complete(other.private_shops_seed_complete),
      private_shop_operation_sources(other.private_shop_operation_sources),
      private_shop_operation_capabilities(
          other.private_shop_operation_capabilities),
      private_shop_household_custody(other.private_shop_household_custody),
      private_shop_daily_rent(other.private_shop_daily_rent),
      private_shop_daily_rent_seed_complete(
          other.private_shop_daily_rent_seed_complete),
      private_shop_command_results(other.private_shop_command_results),
      private_shop_command_results_seed_complete(
          other.private_shop_command_results_seed_complete),
      private_shop_liquidations(other.private_shop_liquidations),
      private_shop_liquidations_seed_complete(
          other.private_shop_liquidations_seed_complete),
      political_shop_licensing(other.political_shop_licensing),
      political_shop_licensing_seed_complete(
          other.political_shop_licensing_seed_complete),
      political_shop_licensing_receipts(
          other.political_shop_licensing_receipts),
      political_governance_continuations(
          other.political_governance_continuations),
      governance_invitation_replies(other.governance_invitation_replies),
      political_head_handovers(other.political_head_handovers),
      political_successor_rounds(other.political_successor_rounds),
      political_term_handovers(other.political_term_handovers),
      political_office_receptions(other.political_office_receptions),
      political_office_resignations(other.political_office_resignations),
      political_subordinate_appointments(
          other.political_subordinate_appointments),
      political_dismissal_sources(other.political_dismissal_sources),
      political_governance_agencies(other.political_governance_agencies),
      political_successions(other.political_successions),
      political_shop_registry_successions(
          other.political_shop_registry_successions),
      political_shop_registry_successions_seed_complete(
          other.political_shop_registry_successions_seed_complete),
      growth_market_capture(other.growth_market_capture),
      local_economy(other.local_economy),
      economic_obligation_sources(other.economic_obligation_sources),
      organization(other.organization),
      organization_runtime(other.organization_runtime),
      governance_programs(other.governance_programs), affairs(other.affairs),
      director(other.director), population_military(other.population_military),
      diplomacy(other.diplomacy),
      diplomacy_seed_complete(other.diplomacy_seed_complete),
      treaty_passage(other.treaty_passage),
      treaty_passage_seed_complete(other.treaty_passage_seed_complete),
      political_takeover(other.political_takeover),
      political_takeover_seed_complete(other.political_takeover_seed_complete),
      economic_takeover_execution(other.economic_takeover_execution),
      economic_takeover_seed_complete(other.economic_takeover_seed_complete),
      politics(other.politics), political_fiscal(other.political_fiscal),
      political_military_appointments(other.political_military_appointments),
      political_military_appointments_seed_complete(
          other.political_military_appointments_seed_complete),
      political_field_appointments(other.political_field_appointments),
      political_field_appointments_seed_complete(
          other.political_field_appointments_seed_complete),
      politics_governance(other.politics_governance),
      politics_governance_seed_complete(
          other.politics_governance_seed_complete),
      fiscal_settlement(other.fiscal_settlement),
      fiscal_settlement_seed_complete(other.fiscal_settlement_seed_complete),
      fiscal_taxpayer_continuity(other.fiscal_taxpayer_continuity),
      fiscal_taxpayer_continuity_seed_complete(
          other.fiscal_taxpayer_continuity_seed_complete),
      household_provision_management(other.household_provision_management),
      household_provision_management_seed_complete(
          other.household_provision_management_seed_complete),
      production_disruptions(other.production_disruptions),
      production_disruptions_seed_complete(
          other.production_disruptions_seed_complete),
      public_relief(other.public_relief),
      public_relief_seed_complete(other.public_relief_seed_complete),
      politics_seed_complete(other.politics_seed_complete),
      political_fiscal_seed_complete(other.political_fiscal_seed_complete),
      occupation_fiscal(other.occupation_fiscal),
      occupation_fiscal_seed_complete(other.occupation_fiscal_seed_complete),
      occupation_fiscal_settlement(other.occupation_fiscal_settlement),
      occupation_fiscal_settlement_seed_complete(
          other.occupation_fiscal_settlement_seed_complete),
      player_plan(other.player_plan),
      player_plan_seed_complete(other.player_plan_seed_complete),
      population_seed_complete(other.population_seed_complete),
      dynamic_seed_complete(other.dynamic_seed_complete), services(*this) {
  common.bind_membership(&services, bool(catalog->field_battle));
  common.bind_task_owners(&services);
  common.bind_family_identity(&services);
  common.bind_household_owners(&services);
  common.bind_organization_owners(&services);
  common.bind_reservation_owners(&services);
  common.bind_protected_market_resources(services.market_history());
  observation_index = build_world_observation_index(common, social, tasks);
  map_presence_index = build_map_presence_index(common);
  economic_index = rebuild_economic_world_index(*this);
  fiscal_index = rebuild_economic_fiscal_index(*this);
  household_provision_index = rebuild_household_provision_index(*this);
  politics_view_index = make_politics_view_index(
      common, politics, organization,
      catalog->organization ? *catalog->organization : OrganizationCatalog{},
      social, &political_fiscal);
  governance_view_index = make_governance_view_index(
      common, politics_governance, tasks, governance_tasks);
  rebuild_diplomacy_index();
}
WorldRead WorldBundle::read() const {
  return {common,
          travel,
          military,
          interaction,
          social,
          estate,
          tasks,
          family,
          personal,
          growth,
          dynamic_enabled() ? &local_economy : nullptr,
          dynamic_enabled() ? &organization : nullptr,
          dynamic_enabled() ? &affairs : nullptr,
          dynamic_enabled() ? &director : nullptr,
          population_military_enabled() ? &population_military : nullptr,
          politics_enabled() ? &politics : nullptr,
          political_fiscal_enabled() ? &political_fiscal : nullptr,
          fiscal_settlement_enabled() ? &fiscal_settlement : nullptr,
          politics_governance_enabled() ? &politics_governance : nullptr,
          governance_tasks_enabled() ? &governance_tasks : nullptr,
          public_relief_enabled() ? &public_relief : nullptr,
          diplomacy_enabled() ? &diplomacy : nullptr,
          envoy_tasks_enabled() ? &envoy_tasks : nullptr,
          household_provision_management_enabled(*this)
              ? &household_provision_management
              : nullptr,
          production_disruptions_enabled() ? &production_disruptions : nullptr};
}
Txn WorldBundle::write() {
  note_write_boundary();
  return {common,
          travel,
          military,
          interaction,
          social,
          estate,
          tasks,
          family,
          personal,
          growth,
          dynamic_enabled() ? &local_economy : nullptr,
          dynamic_enabled() ? &organization : nullptr,
          dynamic_enabled() ? &affairs : nullptr,
          dynamic_enabled() ? &director : nullptr,
          population_military_enabled() ? &population_military : nullptr,
          politics_enabled() ? &politics : nullptr,
          political_fiscal_enabled() ? &political_fiscal : nullptr,
          fiscal_settlement_enabled() ? &fiscal_settlement : nullptr,
          politics_governance_enabled() ? &politics_governance : nullptr,
          governance_tasks_enabled() ? &governance_tasks : nullptr,
          public_relief_enabled() ? &public_relief : nullptr,
          diplomacy_enabled() ? &diplomacy : nullptr,
          envoy_tasks_enabled() ? &envoy_tasks : nullptr,
          household_provision_management_enabled(*this)
              ? &household_provision_management
              : nullptr,
          production_disruptions_enabled() ? &production_disruptions : nullptr};
}
// 适配器只借当前完整候选，调用方必须先检查能力已启用。
PopulationMilitaryReadContext
WorldBundle::population_read(std::optional<Integer> finish) const {
  return {common,
          local_economy,
          *catalog->local_economy,
          organization,
          *catalog->organization,
          military,
          *catalog->military,
          population_military,
          *catalog->population_military,
          finish,
          naval_combat_enabled() ? &naval : nullptr,
          recruitment_occupation_reader(*this, finish),
          recruitment_occupation_history_reader(*this, finish)};
}
PopulationMilitaryWriteContext
WorldBundle::population_write(std::optional<Integer> finish) {
  return {common,
          local_economy,
          *catalog->local_economy,
          organization,
          *catalog->organization,
          military,
          *catalog->military,
          population_military,
          *catalog->population_military,
          finish,
          naval_combat_enabled() ? &naval : nullptr,
          recruitment_occupation_reader(*this, finish),
          recruitment_occupation_history_reader(*this, finish)};
}
PoliticsReadContext
WorldBundle::politics_read(const PoliticalNavigationRead *navigation) const {
  PoliticsReadContext out{common,
                          *catalog->common,
                          politics,
                          *catalog->politics,
                          organization,
                          *catalog->organization,
                          local_economy,
                          social,
                          services,
                          navigation,
                          &politics_view_index};
  if (catalog->political_dismissal_sources)
    out.dismissal_preview = [this](const StableId &actor,
                                   const PoliticalDismissCommand &command) {
      return preview_political_dismissal_world(*this, actor, command);
    };
  out.original_world = this;
  return out;
}
PoliticsWriteContext
WorldBundle::politics_write(const PoliticalNavigationRead *navigation) {
  PoliticsWriteContext out{common,
                           *catalog->common,
                           politics,
                           *catalog->politics,
                           organization,
                           *catalog->organization,
                           local_economy,
                           social,
                           services,
                           navigation,
                           &politics_view_index};
  if (catalog->political_dismissal_sources) {
    out.dismissal_preview = [this](const StableId &actor,
                                   const PoliticalDismissCommand &command) {
      return preview_political_dismissal_world(*this, actor, command);
    };
    out.dismissal_apply = [this](const StableId &actor,
                                 const PoliticalDismissCommand &command) {
      return commit_political_dismissal_world(*this, actor, command);
    };
  }
  out.candidate_world = this;
  return out;
}
PoliticalFiscalReadContext WorldBundle::political_fiscal_read() const {
  return {common,
          *catalog->common,
          politics,
          political_fiscal,
          *catalog->political_fiscal,
          organization,
          *catalog->organization,
          services};
}
PoliticalFiscalWriteContext WorldBundle::political_fiscal_write() {
  return {common,
          *catalog->common,
          politics,
          political_fiscal,
          *catalog->political_fiscal,
          organization,
          *catalog->organization,
          services};
}
DiplomacyReadContext
WorldBundle::diplomacy_read(const InteractionAccess &ia,
                            const DiplomacyNavigationRead *navigation) const {
  return {common,
          *catalog->common,
          organization,
          *catalog->organization,
          diplomacy,
          *catalog->diplomacy,
          interaction,
          *catalog->interaction,
          ia,
          tasks,
          envoy_tasks,
          services,
          &diplomacy_view_index,
          navigation};
}
DiplomacyWriteContext
WorldBundle::diplomacy_write(const InteractionAccess &ia,
                             const DiplomacyNavigationRead *navigation) {
  return {common,
          *catalog->common,
          organization,
          *catalog->organization,
          diplomacy,
          *catalog->diplomacy,
          interaction,
          *catalog->interaction,
          ia,
          tasks,
          envoy_tasks,
          services,
          [this](const StableId &id) { return release_action(*this, id); },
          &diplomacy_view_index,
          navigation};
}
void WorldBundle::rebuild_diplomacy_index() {
  if (!diplomacy_enabled() || !catalog->diplomacy || !catalog->organization) {
    diplomacy_view_index = {};
    return;
  }
  auto ia = services.interaction_access();
  diplomacy_view_index = make_diplomacy_business_view_index(diplomacy_read(ia));
}
Status WorldBundle::validate() const { return validate_impl(nullptr); }
Status WorldBundle::validate(ProjectionWriter &projection) const {
  return validate_impl(&projection);
}
Status WorldBundle::validate_impl(ProjectionWriter *projection) const {
  MilitarySupplyLookupPass supply_lookup(common.state(), military);
  world_stage_observation::Scope source_observation(world_stage_observation::Stage::world_validate);
  auto passage_layout = validate_native_field_passage_layout(*this);
  if (!passage_layout.ok)
    return passage_layout;
  auto relations = validate_native_political_relations_world(*this);
  if (!relations.ok)
    return relations;
  auto injury_layout = validate_native_character_injury_layout(*this);
  if (!injury_layout.ok)
    return injury_layout;
  {
    auto status = validate_profile_deliveries();
    if (!status.ok)
      return status;
  }
  auto stamp_status = read_stamps->usable();
  if (!stamp_status.ok)
    return stamp_status;
  // 非编排冷验段保有原世界读票；专业编排独立分段，不制造未来时刻或复用旧票。
  std::unique_ptr<PhaseCurrentReadScope> validation_phase;
  if (catalog->exact_time && exact_complete_selected(*catalog->exact_time) &&
      !has_active_read_phase()) {
    auto actual = actual_phase_moment();
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    auto phase =
        enter_phase_read_context("world_validation", "world",
                                 {actual.value, actual.value}, actual.value);
    if (!phase.ok)
      return {false, {}, std::move(phase.error)};
    validation_phase = std::move(phase.value);
  }
  auto custody_layout =
      validate_native_private_shop_household_custody_layout(*this);
  if (!custody_layout.ok)
    return custody_layout;
  auto face_sources = validate_native_face_to_face_world(*this);
  if (!face_sources.ok)
    return face_sources;
  auto interaction_layout =
      validate_native_interaction_layout(*catalog, interaction);
  if (!interaction_layout.ok)
    return interaction_layout;
  auto transport_layout = validate_native_transport_layout(*this);
  if (!transport_layout.ok)
    return transport_layout;
  auto boundary_layout = validate_native_movement_boundary_layout(*this);
  if (!boundary_layout.ok)
    return boundary_layout;
  auto continuation_layout = validate_native_movement_boundary_continuation_layout(*this);
  if (!continuation_layout.ok)
    return continuation_layout;
  auto local_move_layout = validate_native_local_move_layout(*this);
  if (!local_move_layout.ok)
    return local_move_layout;
  auto group_handoff_layout = validate_native_group_member_handoff_layout(*this);
  if (!group_handoff_layout.ok)
    return group_handoff_layout;
  auto production_layout = validate_native_production_cycles_layout(*this);
  if (!production_layout.ok)
    return production_layout;
  auto funding_layout = validate_native_private_transport_funding_layout(*this);
  if (!funding_layout.ok)
    return funding_layout;
  auto control_layout = validate_native_control_continuation_layout(*this);
  if (!control_layout.ok)
    return control_layout;
  auto organization_layout = validate_native_organization_runtime_layout(*this);
  if (!organization_layout.ok)
    return organization_layout;
  auto obligation_layout = validate_native_economic_obligation_layout(*this);
  if (!obligation_layout.ok)
    return obligation_layout;
  auto birth_layout = validate_native_family_birth_layout(*this);
  if (!birth_layout.ok)
    return birth_layout;
  auto population_layout = validate_native_born_population_layout(*this);
  if (!population_layout.ok)
    return population_layout;
  auto governance_layout = validate_native_governance_program_layout(*this);
  if (!governance_layout.ok)
    return governance_layout;
  auto facility_layout = validate_native_economic_facility_layout(*this);
  if (!facility_layout.ok)
    return facility_layout;
  auto facility_headers = validate_native_economic_facility_headers(
      common, *catalog, &local_economy,
      economic_facility_capacity_sources ? &*economic_facility_capacity_sources
                                         : nullptr,
      production_facility_conditions ? &*production_facility_conditions
                                     : nullptr,
      governance_work_expense ? &*governance_work_expense : nullptr);
  if (!facility_headers.ok)
    return facility_headers;
  auto field_battle_layout = validate_field_battle_layout(
      catalog->identity, catalog->field_battle.get(),
      field_battle ? &*field_battle : nullptr);
  if (!field_battle_layout.ok)
    return field_battle_layout;
  // 未选 Field 也反扫 Common 孤回执；helper 内只在真已选时构建原域图。
  auto field_sources = validate_native_field_world_sources(*this);
  if (!field_sources.ok)
    return field_sources;
  auto local_move_sources = validate_native_local_move_world_sources(*this);
  if (!local_move_sources.ok)
    return local_move_sources;
  if (catalog->exact_time && exact_complete_selected(*catalog->exact_time)) {
    auto custody_sources = validate_native_named_military_custody_world_sources(*this);
    if (!custody_sources.ok) return custody_sources;
  }
  auto military_layout = validate_native_military_state_layout(*this);
  if (!military_layout.ok)
    return military_layout;
  auto life_age_layout = validate_native_life_age_layout(*this);
  if (!life_age_layout.ok)
    return life_age_layout;
  auto permanent = validate_native_growth_permanent_layout(*this);
  if (!permanent.ok)
    return permanent;
  if (catalog->growth->complete_capability_application_chain) {
    auto acquisitions = validate_profile_acquisitions_world(*this);
    if (!acquisitions.ok)
      return acquisitions;
  }
  auto director_catalog = validate_native_director_catalog_selection(*catalog);
  if (!director_catalog.ok)
    return director_catalog;
  auto inspection = validate_native_public_supply_inspection(*this);
  if (!inspection.ok)
    return inspection;
  // 专业闭包自己签发分阶段读票；只退出本函数拥有的外层票，不抢外部 phase。
  const bool complete_validation =
      catalog->exact_time && exact_complete_selected(*catalog->exact_time);
  if (complete_validation)
    validation_phase.reset();
  auto exact = validate_exact_time_world(*this);
  if (!exact.ok)
    return exact;
  // 后半冷验重新取得同一实际 M 的原世界票，早先各专业票均已自然退役。
  if (complete_validation) {
    auto actual = actual_phase_moment();
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    auto phase =
        enter_phase_read_context("world_validation", "world",
                                 {actual.value, actual.value}, actual.value);
    if (!phase.ok)
      return {false, {}, std::move(phase.error)};
    validation_phase = std::move(phase.value);
  }
  auto occupation = validate_native_military_occupation_selection(*this);
  if (!occupation.ok)
    return occupation;
  auto funding = validate_native_private_transport_funding_world(*this);
  if (!funding.ok)
    return funding;
  const std::optional<Integer> unselected_clock =
      catalog->exact_time
          ? std::optional<Integer>{catalog->actor_foundation->actors.rows()
                                       .front()
                                       .second.scenario_origin_ref.initial_tick}
          : std::nullopt;
  const bool exact_supply_source_scope =
      catalog->exact_time && exact_social_selected(*catalog->exact_time);
  auto voluntary_profile = validate_family_voluntary_paths_profile(*this);
  if (!voluntary_profile.ok)
    return voluntary_profile;
  auto daily_pair = validate_private_shop_daily_rent_pair(
      private_shop_daily_rent_enabled(), catalog->identity.module_versions);
  if (!daily_pair.ok)
    return daily_pair;
  if (private_shop_daily_rent_enabled() !=
      private_shop_daily_rent_seed_complete)
    return common_failure("PrivateShopDailyRentSeedRequired");
  auto shop_pair = validate_private_shop_pair(
      private_shops_enabled(), catalog->identity.module_versions);
  if (!shop_pair.ok)
    return shop_pair;
  if (private_shops_enabled() != political_shop_licensing_enabled() ||
      (private_shops_enabled() &&
       (!dynamic_enabled() || !growth_market_history_enabled())) ||
      private_shops_enabled() != private_shops_seed_complete)
    return common_failure("PrivateShopLicensingPairRequired");
  auto officer_layout = validate_political_military_appointment_layout(
      political_military_appointments_enabled(),
      catalog->identity.module_versions,
      political_military_appointments_seed_complete
          ? &political_military_appointments
          : nullptr);
  if (!officer_layout.ok)
    return officer_layout;
  auto officer_history =
      validate_political_military_appointment_command_history(
          common, political_military_appointments_enabled());
  if (!officer_history.ok)
    return officer_history;
  auto field_layout = validate_political_field_appointment_layout(
      political_field_appointments_enabled(), catalog->identity.module_versions,
      political_field_appointments_seed_complete ? &political_field_appointments
                                                 : nullptr);
  if (!field_layout.ok)
    return field_layout;
  auto field_history = validate_political_field_appointment_command_history(
      common, political_field_appointments_enabled());
  if (!field_history.ok)
    return field_history;
  if (catalog->task_message_handoffs_enabled !=
      (catalog->task_message_handoff_version != 0))
    return common_failure("TaskMessageHandoffProfileMismatch");
  auto handoff_layout = validate_task_message_handoff_layout(
      common, catalog->task_message_handoffs_enabled,
      catalog->identity.module_versions,
      tasks.message_handoffs ? &*tasks.message_handoffs : nullptr,
      catalog->task_message_handoffs_enabled
          ? catalog->task_message_handoff_version
          : 1);
  if (!handoff_layout.ok)
    return handoff_layout;
  if (catalog->task_message_handoffs_enabled) {
    if (!dynamic_enabled())
      return common_failure("TaskMessageHandoffProfileMismatch");
    auto valid = validate_task_message_handoffs(common, tasks, *catalog->tasks,
                                                *catalog->common,
                                                *tasks.message_handoffs);
    if (!valid.ok)
      return valid;
  }
  auto takeover_layout = validate_political_takeover_layout(
      political_takeover_enabled(), catalog->identity.module_versions,
      political_takeover_seed_complete ? &political_takeover : nullptr,
      economic_takeover_seed_complete
          ? std::optional<Integer>{economic_takeover_execution.version}
          : std::nullopt,
      tasks.takeover_execution
          ? std::optional<Integer>{tasks.takeover_execution->version}
          : std::nullopt,
      catalog->political_takeover
          ? catalog->political_takeover->task_execution_version
          : 1,
      catalog->political_takeover ? catalog->political_takeover->version : 1);
  if (!takeover_layout.ok)
    return takeover_layout;
  if (catalog->task_takeover_execution_enabled != political_takeover_enabled())
    return common_failure("PoliticalTakeoverProfileMismatch");
  auto task_takeover_layout = validate_task_public_account_execution_layout(
      common, catalog->task_takeover_execution_enabled,
      catalog->identity.module_versions,
      tasks.takeover_execution ? &*tasks.takeover_execution : nullptr);
  if (!task_takeover_layout.ok)
    return task_takeover_layout;
  auto takeover_history = validate_political_takeover_command_history(
      common, political_takeover_enabled());
  if (!takeover_history.ok)
    return takeover_history;
  if (!political_takeover_enabled() &&
      (!political_takeover.registrations.empty() ||
       !political_takeover.approvals.empty() ||
       !political_takeover.decisions.empty() ||
       !economic_takeover_execution.payments.empty() ||
       !economic_takeover_execution.rations.empty() ||
       !economic_takeover_execution.production.empty() ||
       !economic_takeover_execution.configurations.empty() ||
       !economic_takeover_execution.relief.empty() ||
       !economic_takeover_execution.configuration_requests.empty()))
    return common_failure("UnexpectedPoliticalTakeoverState");
  std::unique_ptr<CompleteExactWorldBindingOwner> political_exact_owner;
  std::shared_ptr<const PoliticalExactReadLease> political_exact_lease;
  std::optional<PoliticalExactReadBinding> political_exact_binding;
  std::unique_ptr<PoliticalTakeoverWorldAccess> takeover_access;
  std::optional<PoliticalTakeoverAccess> exact_takeover_access;
  std::optional<PoliticalOccupationFiscalTaxpayerAccess> exact_taxpayer;
  std::optional<EconomicTakeoverReadContext> takeover_economy;
  const PoliticalTakeoverRead *takeover_responsibilities = nullptr;
  const bool selected_takeover2 =
      political_takeover_enabled() && catalog->political_takeover->version == 2;
  const bool selected_fiscal2 =
      political_fiscal_enabled() && catalog->political_fiscal->version == 2;
  if (selected_takeover2 || selected_fiscal2) {
    if (!catalog->exact_time || !exact_complete_selected(*catalog->exact_time))
      return common_failure("PoliticalOriginalCompleteSourcesRequired");
    auto owner = complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, std::move(owner.error)};
    political_exact_owner = std::move(owner.value);
    auto actual = actual_phase_moment();
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    auto lease = political_exact_owner->read_political(actual.value);
    if (!lease.ok)
      return {false, {}, std::move(lease.error)};
    political_exact_lease = std::move(lease.value);
    if (!political_exact_lease)
      return common_failure("PoliticalOriginalReadLeaseRequired");
    political_exact_binding.emplace(political_exact_lease->bindings());
    auto checked = validate_native_political_world_business_sources(
        *this, *political_exact_binding);
    if (!checked.ok)
      return checked;
  }
  if (political_takeover_enabled()) {
    if (selected_takeover2) {
      const auto *context = political_exact_binding->takeover;
      if (!context)
        return common_failure("PoliticalTakeoverOriginalContextRequired");
      exact_takeover_access.emplace(*context);
      takeover_responsibilities = &*exact_takeover_access;
      if (occupation_fiscal_enabled() || fiscal_taxpayer_continuity_enabled())
        exact_taxpayer.emplace(*context);
    } else {
      auto read = PoliticalTakeoverWorldAccess::create(
          *this, *catalog->political_takeover, political_takeover);
      if (!read.ok)
        return {false, {}, read.error};
      takeover_access = std::move(read.value);
      takeover_responsibilities = &takeover_access->responsibilities();
    }
    takeover_economy.emplace(EconomicTakeoverReadContext{
        common, local_economy, *catalog->local_economy,
        fiscal_settlement_enabled() ? &fiscal_settlement : nullptr,
        fiscal_settlement_enabled() ? catalog->fiscal_settlement.get()
                                    : nullptr,
        public_relief_enabled() ? &public_relief : nullptr,
        *takeover_responsibilities, &organization, catalog->organization.get(),
        nullptr,
        selected_takeover2 && occupation_fiscal_enabled()
            ? &occupation_fiscal_settlement
            : nullptr,
        exact_taxpayer ? &*exact_taxpayer : nullptr});
  }

  const auto control_markers =
      std::count_if(catalog->identity.module_versions.begin(),
                    catalog->identity.module_versions.end(), [](const auto &v) {
                      return v.first == "household_control";
                    });
  if (control_markers != (household_control_enabled() ? 1 : 0) ||
      (household_control_enabled() &&
       (!dynamic_enabled() ||
        std::find(
            catalog->identity.module_versions.begin(),
            catalog->identity.module_versions.end(),
            std::make_pair(StableId{"household_control"}, StableId{"1"})) ==
            catalog->identity.module_versions.end())))
    return common_failure("HouseholdControlProfileMismatch");
  // 独立段、显式marker和typed来源同时存在；旧profile的无缓存历史也须拒绝。
  const auto naval_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &row) { return row.first == "military_naval"; });
  if (naval_enabled() != naval_seed_complete ||
      naval_markers != (naval_enabled() ? 1 : 0) ||
      (naval_enabled() &&
       (!dynamic_enabled() || !catalog->common->extended_skills ||
        std::find(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(),
                  std::make_pair(
                      StableId{"military_naval"},
                      std::to_string(naval_codec::version(*catalog->naval)))) ==
            catalog->identity.module_versions.end())))
    return common_failure("NavalProfileMismatch");
  const auto repair_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &row) { return row.first == "naval_repairs"; });
  if (repair_markers != (naval_repairs_enabled() ? 1 : 0) ||
      (naval_repairs_enabled() &&
       (!naval_combat_enabled() ||
        std::find(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(),
                  std::make_pair(StableId{"naval_repairs"}, StableId{"1"})) ==
            catalog->identity.module_versions.end())))
    return common_failure("NavalRepairProfileMismatch");
  const auto charge_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &row) { return row.first == "naval_repair_charges"; });
  if (charge_markers != (naval_repair_charges_enabled() ? 1 : 0) ||
      (naval_repair_charges_enabled() &&
       std::find(
           catalog->identity.module_versions.begin(),
           catalog->identity.module_versions.end(),
           std::make_pair(StableId{"naval_repair_charges"}, StableId{"1"})) ==
           catalog->identity.module_versions.end()))
    return common_failure("NavalRepairChargeProfileMismatch");
  const auto navigation_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &row) { return row.first == "naval_navigation_wear"; });
  if (navigation_markers != (naval_navigation_enabled() ? 1 : 0) ||
      (naval_navigation_enabled() &&
       std::find(
           catalog->identity.module_versions.begin(),
           catalog->identity.module_versions.end(),
           std::make_pair(StableId{"naval_navigation_wear"}, StableId{"1"})) ==
           catalog->identity.module_versions.end()))
    return common_failure("NavalNavigationProfileMismatch");
  const auto gunnery_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &row) { return row.first == "naval_gunnery"; });
  if (gunnery_markers != (naval_gunnery_enabled() ? 1 : 0) ||
      (naval_gunnery_enabled() &&
       std::find(catalog->identity.module_versions.begin(),
                 catalog->identity.module_versions.end(),
                 std::make_pair(StableId{"naval_gunnery"}, StableId{"1"})) ==
           catalog->identity.module_versions.end()) ||
      (!naval_gunnery_enabled() && (!naval.gunnery.preparations.empty() ||
                                    !naval.gunnery.field_order.empty())))
    return common_failure("NavalGunneryProfileMismatch");
  const bool tactical_navigation =
      naval_combat_enabled() &&
      catalog->naval->combat_policy->combat.tactical_navigation.has_value();
  const auto tactical_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &row) { return row.first == "naval_tactical_navigation"; });
  if (tactical_markers != (tactical_navigation ? 1 : 0) ||
      (tactical_navigation &&
       std::find(catalog->identity.module_versions.begin(),
                 catalog->identity.module_versions.end(),
                 std::make_pair(StableId{"naval_tactical_navigation"},
                                StableId{"1"})) ==
           catalog->identity.module_versions.end()))
    return common_failure("NavalTacticalNavigationProfileMismatch");
  if (!naval_enabled() && (!naval.vessels.empty() || !naval.fleets.empty()))
    return common_failure("UnexpectedNavalState");
  for (const auto &entry : common.state().history)
    if ((entry.kind == "naval_form" || entry.kind == "naval_disband") &&
        !naval_enabled())
      return common_failure("NavalProfileRequired",
                            "history/" + entry.result_id);
  for (const auto &cached : common.state().results.rows()) {
    if (!cached.second)
      continue;
    const auto &result = *cached.second;
    const bool kind =
        result.kind == "naval_form" || result.kind == "naval_disband";
    const auto *receipt =
        result.details ? std::get_if<NavalCommandReceipt>(&result.details->data)
                       : nullptr;
    if (!kind && !receipt)
      continue;
    if (!naval_enabled() || !kind || !receipt || receipt->kind != result.kind)
      return common_failure("UnexpectedNavalReceipt", cached.first);
    StableId actor;
    for (const auto &h : common.state().history)
      if (h.result_id == result.result_id) {
        if (!actor.empty())
          return common_failure("NavalCachedHistoryMismatch", cached.first);
        actor = h.actor_id;
      }
    auto checked = naval_command_codec::validate_cached_receipt(
        military, naval, *receipt, actor);
    if (!checked.ok)
      return checked;
  }
  if (!naval_repairs_enabled()) {
    auto naval_history = validate_naval_command_history(*this);
    if (!naval_history.ok)
      return naval_history;
  }
  // 公共实例计数只读核验；域不另设ID生成器或修复坏银行。
  auto naval_instance = [&](const StableId &id) {
    const auto colon = id.find(':');
    if (colon == StableId::npos)
      return false;
    auto digits = id.substr(colon + 1);
    Integer n = 0;
    auto parsed =
        std::from_chars(digits.data(), digits.data() + digits.size(), n);
    return parsed.ec == std::errc{} &&
           parsed.ptr == digits.data() + digits.size() && n > 0 &&
           n < common.state().next_instance_id && std::to_string(n) == digits;
  };
  for (const auto &fleet : naval.fleets)
    if (!naval_instance(fleet.id) || !naval_instance(fleet.source_id) ||
        (fleet.status == "released" &&
         !naval_instance(fleet.release_source_id)))
      return common_failure("InstanceCounterCollision", fleet.id);

  auto diplomacy_layout = validate_diplomacy_layout(
      diplomacy_enabled(), catalog->identity.module_versions,
      diplomacy_seed_complete, envoy_tasks_seed_complete,
      catalog->diplomacy ? catalog->diplomacy->version : 1);
  if (!diplomacy_layout.ok)
    return diplomacy_layout;
  if (diplomacy_enabled() &&
      (!dynamic_enabled() || !catalog->diplomacy || !catalog->organization))
    return common_failure("IncompleteDiplomacyProfile");
  auto envoy_version = validate_envoy_task_version_boundary(
      common, envoy_tasks_enabled() ? catalog->diplomacy->version : 0,
      envoy_tasks_enabled() ? &envoy_tasks : nullptr);
  if (!envoy_version.ok)
    return envoy_version;
  if (!diplomacy_enabled() &&
      (catalog->diplomacy || !diplomacy.offers.empty() ||
       !diplomacy.facts.empty() || !diplomacy.presentations.empty() ||
       !diplomacy.replies.empty() || !diplomacy.missions.empty() ||
       diplomacy.v2.has_value()))
    return common_failure("UnexpectedDiplomacyState");
  auto diplomacy_history = validate_diplomacy_command_history(
      common, diplomacy_enabled(),
      catalog->diplomacy ? catalog->diplomacy->version : 1);
  if (!diplomacy_history.ok)
    return diplomacy_history;
  if (diplomacy_enabled()) {
    auto profile = validate_diplomacy_runtime_profile(*catalog->diplomacy);
    if (!profile.ok)
      return profile;
    DiplomaticMissionAccess missions(common, organization,
                                     *catalog->organization,
                                     *catalog->diplomacy, diplomacy);
    std::optional<DiplomaticReportAccess> reports;
    if (diplomacy.v2)
      reports.emplace(DiplomaticHistoryRead{common, organization,
                                            *catalog->organization,
                                            *catalog->diplomacy, diplomacy},
                      *diplomacy.v2);
    auto envoy_read =
        reports ? EnvoyTaskRead(common, tasks, envoy_tasks, missions, missions,
                                *reports)
                : EnvoyTaskRead(common, tasks, envoy_tasks, missions, missions);
    auto valid =
        validate_diplomacy_state(common, organization, *catalog->organization,
                                 *catalog->diplomacy, diplomacy, &envoy_read);
    if (!valid.ok)
      return valid;
  }
  // 缓存已退役的历史也不能为未启用请求的旧profile夹入新命令。
  if (!dynamic_enabled() || !catalog->military ||
      !catalog->military->request_policy)
    for (const auto &entry : common.state().history)
      if (entry.kind == "military_request")
        return common_failure("MilitaryRequestPolicyRequired",
                              "history/" + entry.result_id);
  // 共同缓存只引用唯一请求事实；旧profile或异类命令不能夹带新答复。
  for (const auto &cached : common.state().results.rows()) {
    if (!cached.second)
      continue;
    const auto &result = *cached.second;
    const auto *effect =
        result.details ? std::get_if<MilitaryEffect>(&result.details->data)
                       : nullptr;
    const bool answer = effect && effect->request_answer;
    if (result.kind != "military_request" && !answer)
      continue;
    if (!dynamic_enabled() || !catalog->military ||
        !catalog->military->request_policy ||
        result.kind != "military_request" || !answer ||
        effect->receipt_id != effect->request_answer->receipt_id ||
        effect->battle_id != effect->request_answer->battle_id)
      return common_failure("UnexpectedMilitaryRequestReceipt", cached.first);
    StableId actor;
    for (const auto &entry : common.state().history)
      if (entry.result_id == result.result_id) {
        if (!actor.empty())
          return common_failure("MilitaryRequestCachedHistoryMismatch",
                                cached.first);
        actor = entry.actor_id;
      }
    auto linked = validate_military_request_cached_answer(
        military, *effect->request_answer, actor);
    if (!linked.ok)
      return linked;
  }

  if (public_relief_enabled() && !catalog->public_relief)
    return common_failure("MissingPublicReliefCatalog");
  if (public_relief_enabled() && !public_relief_seed_complete)
    return common_failure("MissingPublicReliefState");
  if (!public_relief_enabled() &&
      (public_relief_seed_complete || !public_relief.commitments.empty() ||
       !public_relief.receipts.empty() || !public_relief.releases.empty()))
    return common_failure("UnexpectedPublicReliefState");
  if (politics_governance_enabled() &&
      (!politics_enabled() || !catalog->politics_governance ||
       !catalog->politics_governance->enabled || !catalog->public_relief))
    return common_failure("IncompleteGovernanceProfile");
  if (politics_governance_enabled() && !politics_governance_seed_complete)
    return common_failure("MissingPoliticsGovernanceState");
  if (!politics_governance_enabled() &&
      (catalog->politics_governance || politics_governance_seed_complete ||
       !politics_governance.authorities.empty() ||
       !politics_governance.plans.empty() ||
       !politics_governance.civil.empty() ||
       !politics_governance.consequences.empty()))
    return common_failure("UnexpectedPoliticsGovernanceState");
  {
    auto managed = validate_household_provision_world(*this);
    if (!managed.ok)
      return managed;
  }
  if (production_disruptions_enabled()) {
    if (!production_disruptions_seed_complete)
      return common_failure("MissingProductionDisruptionState");
    auto checked = validate_production_disruptions(
        common, local_economy, *catalog->local_economy,
        *catalog->production_disruptions, production_disruptions);
    if (!checked.ok)
      return checked;
  } else if (catalog->production_disruptions ||
             production_disruptions_seed_complete ||
             production_disruptions.version != 1 ||
             !production_disruptions.registrations.empty() ||
             !production_disruptions.settlements.empty())
    return common_failure("UnexpectedProductionDisruptionState");
  if (catalog->fiscal_settlement && !political_fiscal_enabled())
    return common_failure("FiscalSettlementRequiresPoliticalFiscal");
  if (fiscal_settlement_enabled() && !fiscal_settlement_seed_complete)
    return common_failure("MissingFiscalSettlementState");
  auto continuity_layout = validate_fiscal_taxpayer_continuity_layout(
      catalog->fiscal_taxpayer_continuity_enabled,
      catalog->identity.module_versions);
  if (!continuity_layout.ok)
    return continuity_layout;
  if (catalog->fiscal_taxpayer_continuity_enabled !=
          fiscal_taxpayer_continuity_enabled() ||
      fiscal_taxpayer_continuity_enabled() !=
          fiscal_taxpayer_continuity_seed_complete)
    return common_failure("FiscalTaxpayerContinuityProfileMismatch");
  if (!fiscal_taxpayer_continuity_enabled() &&
      (!fiscal_taxpayer_continuity.suspended_periods.empty() ||
       fiscal_taxpayer_continuity.initialized_tick ||
       fiscal_taxpayer_continuity.last_step_tick))
    return common_failure("UnexpectedFiscalTaxpayerContinuityState");
  if (!fiscal_settlement_enabled() && (fiscal_settlement_seed_complete ||
                                       !fiscal_settlement.districts.empty() ||
                                       !fiscal_settlement.remainders.empty() ||
                                       !fiscal_settlement.periods.empty() ||
                                       !fiscal_settlement.assessments.empty() ||
                                       !fiscal_settlement.payments.empty()))
    return common_failure("UnexpectedFiscalSettlementState");
  if (politics_enabled() && !politics_seed_complete)
    return common_failure("MissingPoliticsState");
  if (political_fiscal_enabled() && !political_fiscal_seed_complete)
    return common_failure("MissingPoliticalFiscalState");
  if (catalog->political_fiscal && catalog->political_fiscal->enabled &&
      !politics_enabled())
    return common_failure("PoliticalFiscalRequiresPolitics");
  if (!politics_enabled() &&
      (!politics.jurisdictions.empty() || !politics.appointments.empty() ||
       !politics.facts.empty()))
    return common_failure("UnexpectedPoliticsState");
  if (!political_fiscal_enabled() && !political_fiscal.policies.empty())
    return common_failure("UnexpectedPoliticalFiscalState");
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      if (is_politics_command_kind(row.second->kind) && !politics_enabled())
        return common_failure("PoliticalProfileRequired");
      if (row.second->kind == "political_set_tax" &&
          !political_fiscal_enabled())
        return common_failure("PoliticalFiscalProfileRequired");
    }

  if (dynamic_enabled() &&
      (!dynamic_seed_complete || !catalog->local_economy ||
       !catalog->organization || !catalog->affairs || !catalog->director))
    return common_failure("MissingDynamicWorldState");
  if (population_military_enabled() && !population_seed_complete)
    return common_failure("MissingPopulationMilitaryState");
  if (!population_military_enabled() &&
      (!population_military.enrollments.empty() ||
       !population_military.actions.empty() ||
       !population_military.action_facts.empty() ||
       !population_military.facts.empty() ||
       !local_economy.population_losses.empty()))
    return common_failure("UnexpectedPopulationMilitaryState");
  if (dynamic_enabled() !=
      (common.state().metadata.ruleset_id == dynamic_ruleset_id))
    return common_failure("DynamicProfileMismatch");
  if (!regional_life_enabled())
    for (const auto &row : common.state().results.rows())
      if (row.second &&
          (row.second->kind == "ask_news" || row.second->kind == "talk_actor"))
        return common_failure("RegionalLifeCapabilityRequired", row.first);
  {
    auto social_layout =
        validate_native_social_state_selection(social, *catalog);
    if (!social_layout.ok)
      return social_layout;
  }
  if (player_plan_enabled() && !player_plan_seed_complete)
    return common_failure("MissingPlayerPlanState");
  {
    auto selected_plan = validate_player_plan_selected_version(
        player_plan, catalog->identity, player_plan_enabled(),
        catalog->exact_time && exact_complete_selected(*catalog->exact_time));
    if (!selected_plan.ok)
      return selected_plan;
  }
  if (!player_plan_enabled()) {
    if (player_plan.current)
      return common_failure("UnexpectedPlayerPlanState");
    for (const auto &row : common.state().results.rows())
      if (row.second && player_plan_command_kind(row.second->kind))
        return common_failure("PlayerPlanProfileRequired");
  }
  if (!catalog->growth->study_teaching_requests.enabled) {
    for (const auto &row : common.state().actions.rows())
      if (row.second.kind == "teach_study")
        return common_failure("StudyTeachingProfileRequired",
                              "actions/" + row.first);
    for (const auto &entry : common.state().history)
      if (entry.kind == "teach_study")
        return common_failure("StudyTeachingProfileRequired",
                              "history/" + entry.result_id);
    for (const auto &row : common.state().results.rows())
      if (row.second && row.second->kind == "teach_study")
        return common_failure("StudyTeachingProfileRequired",
                              "results/" + row.first);
  }
  const bool adult_adoption =
      catalog->family && catalog->family->adult_adoption.has_value();
  const auto adult_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &value) { return value.first == "adult_adoption"; });
  if (adult_markers != (adult_adoption ? 1 : 0) ||
      (adult_adoption &&
       (!dynamic_enabled() ||
        std::find(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(),
                  std::make_pair(StableId{"adult_adoption"}, StableId{"1"})) ==
            catalog->identity.module_versions.end())))
    return common_failure("AdultAdoptionProfileMismatch");
  if (!adult_adoption) {
    for (const auto &row : common.state().actions.rows())
      if (row.second.kind == "family_adopt_existing")
        return common_failure("AdultAdoptionCapabilityRequired");
    for (const auto &entry : common.state().history)
      if (entry.kind == "family_adopt_existing")
        return common_failure("AdultAdoptionCapabilityRequired");
    for (const auto &row : common.state().results.rows())
      if (row.second && row.second->kind == "family_adopt_existing")
        return common_failure("AdultAdoptionCapabilityRequired");
  }
  // 未启用新域时连结果与历史里的命令痕迹也必须拒绝，防止藏入旧profile。
  const bool voluntary =
      catalog->family && catalog->family->voluntary_succession.has_value();
  const auto voluntary_markers = std::count_if(
      catalog->identity.module_versions.begin(),
      catalog->identity.module_versions.end(),
      [](const auto &value) { return value.first == "voluntary_succession"; });
  if (voluntary_markers != (voluntary ? 1 : 0) ||
      (voluntary && (!dynamic_enabled() ||
                     std::find(catalog->identity.module_versions.begin(),
                               catalog->identity.module_versions.end(),
                               std::make_pair(StableId{"voluntary_succession"},
                                              StableId{"1"})) ==
                         catalog->identity.module_versions.end())))
    return common_failure("VoluntarySuccessionProfileMismatch");
  const auto succession_command = [](const StableId &kind) {
    return kind == "family_agree_succession" ||
           kind == "family_cancel_succession" || kind == "family_continue_as";
  };
  if (!voluntary) {
    for (const auto &row : common.state().actions.rows())
      if (succession_command(row.second.kind))
        return common_failure("VoluntarySuccessionCapabilityRequired");
    for (const auto &entry : common.state().history)
      if (succession_command(entry.kind))
        return common_failure("VoluntarySuccessionCapabilityRequired");
    for (const auto &row : common.state().results.rows())
      if (row.second && succession_command(row.second->kind))
        return common_failure("VoluntarySuccessionCapabilityRequired");
  }
  auto birth_identity = validate_family_birth_identity_world(*this);
  if (!birth_identity.ok)
    return birth_identity;
  auto valid = common.validate_common();
  if (!valid.ok)
    return valid;
  // 已准备的地图/军旅模型来自固定场景，恢复不能只换场景ID而沿用旧模型开关。
  if (common.state().metadata.scenario_id != catalog->military->scenario_id)
    return common_failure("ResidentScenarioMismatch");
  valid = validate_payload_registry(*this, projection);
  if (!valid.ok)
    return valid;
  valid = validate_travel_domain(common, travel, *catalog->travel, services);
  if (!valid.ok)
    return valid;
  auto ia = services.interaction_access();
  auto ma = services.military_access();
  ma.interaction_access = &ia;
  valid = validate_interaction_state(common, interaction, *catalog->interaction,
                                     ia);
  if (!valid.ok)
    return valid;
  valid = validate_native_actor_social_occupancy(*this);
  if (!valid.ok)
    return valid;
  // Complete 已有原社交时钟与真期间闭包；只复用实际 M 校验，不套未选全空门。
  if (complete_validation) {
    auto actual = actual_phase_moment();
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    valid = validate_social_at_moment(common, social, *catalog->common,
                                      &services, actual.value);
  } else {
    valid = validate_social(common, social, *catalog->common, &services,
                            unselected_clock, exact_supply_source_scope);
  }
  if (!valid.ok)
    return valid;
  auto ea = services.estate_access();
  valid =
      validate_estate(common, estate, *catalog->estate, *catalog->common, &ea);
  if (!valid.ok)
    return valid;
  TaskBattleRead task_battles =
      [this](const StableId &id) -> std::optional<TaskProtectedBattle> {
    const auto *battle = personal.battles.find(id);
    if (!battle)
      return std::nullopt;
    TaskProtectedBattle result;
    result.id = battle->id;
    result.controller_id = battle->controller_id;
    result.outcome = battle->outcome;
    result.applied = battle->applied;
    for (const auto &row : battle->units.rows())
      if (row.second.protected_actor)
        result.protected_actor_ids.push_back(row.first);
    return result;
  };
  if (envoy_tasks_enabled()) {
    if (!envoy_tasks_seed_complete)
      return common_failure("MissingEnvoyTasksState");
    if (!catalog->diplomacy || !catalog->organization)
      return common_failure("MissingEnvoyCatalog");
    if (governance_tasks_enabled() != governance_tasks_seed_complete)
      return common_failure("GovernanceTaskProfileMismatch");
    DiplomaticMissionAccess missions(common, organization,
                                     *catalog->organization,
                                     *catalog->diplomacy, diplomacy);
    auto envoy_access = services.interaction_access();
    DiplomaticPresentRead report_context{
        common,    organization, *catalog->organization, *catalog->diplomacy,
        diplomacy, interaction,  *catalog->interaction,  envoy_access};
    std::optional<DiplomaticReportAccess> report_source;
    if (envoy_tasks.version == 2) {
      if (!diplomacy.v2)
        return common_failure("MissingDiplomacyV2State");
      report_source.emplace(report_context, *diplomacy.v2);
    }
    valid = validate_tasks_with_responsibilities(
        common, tasks, *catalog->tasks, *catalog->common, governance_tasks,
        governance_tasks_enabled(), envoy_tasks, true, missions, missions,
        task_battles, report_source ? &*report_source : nullptr,
        takeover_responsibilities);
    if (!valid.ok)
      return valid;
  } else {
    if (envoy_tasks_seed_complete ||
        !envoy_tasks.envoy_contracts.rows().empty())
      return common_failure("UnexpectedEnvoyTasksState");
    if (governance_tasks_enabled()) {
      if (!governance_tasks_seed_complete)
        return common_failure("MissingGovernanceTasksState");
      valid = validate_tasks_with_governance(
          common, tasks, *catalog->tasks, *catalog->common, governance_tasks,
          true, task_battles, takeover_responsibilities);
    } else {
      if (governance_tasks_seed_complete ||
          !governance_tasks.governance_contracts.empty())
        return common_failure("UnexpectedGovernanceTasksState");
      valid = validate_tasks(common, tasks, *catalog->tasks, *catalog->common,
                             task_battles, takeover_responsibilities);
    }
    if (!valid.ok)
      return valid;
  }
  std::function<Status(const ActorRecord &)> biological_identity =
      [this](const ActorRecord &actor) {
        return services.validate_dynamic_actor(common, actor);
      };
  auto family_sources = NativeFamilyInitialSourcesLease::create(*this);
  if (!family_sources.ok)
    return {false, {}, std::move(family_sources.error)};
  valid = validate_family(
      common, family, *catalog->family, *catalog->common, estate,
      *catalog->estate, &ea, dynamic_enabled() ? &local_economy : nullptr,
      catalog->interaction.get(),
      family_legacy_enabled() ? &family_legacy : nullptr,
      family_birth_enabled() ? &biological_identity : nullptr,
      family_sources.value ? &family_sources.value->origins() : nullptr,
      family_sources.value ? &family_sources.value->initial_age() : nullptr);
  if (!valid.ok)
    return valid;
  valid = validate_party_invitation_layout(
      party_invitations_enabled(), catalog->identity.module_versions,
      catalog->interaction->party_invitations.get());
  if (!valid.ok)
    return valid;
  valid =
      validate_party_invitation_history(common, party_invitations_enabled());
  if (!valid.ok)
    return valid;
  if (party_invitations_enabled()) {
    valid = validate_party_invitation_catalog(
        *catalog->interaction->party_invitations, *catalog->interaction);
    if (!valid.ok)
      return valid;
  }
  valid = validate_party_recruitment_consent_layout(
      catalog->identity.module_versions,
      catalog->interaction->party_recruitment_consents.get());
  if (!valid.ok)
    return valid;
  if (catalog->interaction->party_recruitment_consents) {
    valid = validate_party_recruitment_consent_catalog(
        *catalog->interaction->party_recruitment_consents,
        *catalog->interaction);
    if (!valid.ok)
      return valid;
  }
  valid = validate_visit_message_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_visit_reception_world(*this);
  if (!valid.ok)
    return valid;
  if (takeover_responsibilities) {
    valid = validate_task_public_account_payments(
        common, tasks, *catalog->tasks, *tasks.takeover_execution,
        *takeover_responsibilities);
    if (!valid.ok)
      return valid;
  }
  valid = validate_task_personal_advance_layout(
      bool(catalog->task_personal_advances), catalog->identity.module_versions,
      tasks.personal_advances ? &*tasks.personal_advances : nullptr);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_advance_history(
      common, bool(catalog->task_personal_advances));
  if (!valid.ok)
    return valid;
  if (tasks.personal_advances) {
    valid = validate_task_personal_advance_catalog_links(
        *catalog->task_personal_advances, *catalog->tasks,
        *tasks.personal_advances);
    if (!valid.ok)
      return valid;
  }
  valid = validate_task_offer_response_version_history(
      common, catalog->task_offer_response_version);
  if (!valid.ok)
    return valid;
  valid = validate_task_offer_response_version_layout(
      catalog->task_offer_response_version, catalog->identity.module_versions,
      tasks.offer_responses ? &*tasks.offer_responses : nullptr);
  if (!valid.ok)
    return valid;
  if (tasks.offer_responses) {
    valid = validate_task_offer_responses_with_runtime(
        common, tasks, *catalog->tasks, affairs, social, interaction,
        *catalog->interaction, *tasks.offer_responses);
    if (!valid.ok)
      return valid;
  }
  valid = validate_personal_simultaneous_outcome_modules(
      catalog->identity.module_versions,
      !catalog->personal->simultaneous_outcome_objectives.empty());
  if (!valid.ok)
    return valid;
  valid = validate_personal(common, personal, *catalog->personal, interaction);
  if (!valid.ok)
    return valid;
  valid = validate_native_personal_battle_lifecycle_sources(*this);
  if (!valid.ok)
    return valid;
  valid = validate_native_personal_initial_attributes_sources(*this);
  if (!valid.ok)
    return valid;
  auto ga = services.growth_access();
  if (!catalog->growth->innate_learning) {
    valid = validate_selected_growth_state(*this);
    if (!valid.ok)
      return valid;
  }
  const bool field_commander_losses =
      military_field_commander_profile_in_use(military, *catalog->military);
  if (player_plan_enabled()) {
    valid =
        (growth_tactic_lessons_enabled() || family_voluntary_paths_enabled() ||
         catalog->task_personal_messages || catalog->task_personal_reports)
            ? validate_player_plan_shape(player_plan)
            : validate_player_plan(common, player_plan);
    if (!valid.ok)
      return valid;
    if (!growth_tactic_lessons_enabled() && !catalog->contact_inquiries &&
        !catalog->personal_appointments && !private_shops_enabled() &&
        !personal_recovery_enabled() && !family_birth_enabled() &&
        !naval_combat_enabled() && !field_commander_losses) {
      if (!family_disciple_legacy_enabled() &&
          !family_biological_legacy_enabled() &&
          !family_collateral_legacy_enabled()) {
        if (!military_commanded_damage_enabled()) {
          if (!catalog->visit_appointment_invitations) {
            if (!private_shop_command_results_enabled()) {
              if ((!catalog->task_shop_order_contacts_enabled &&
                   !catalog->task_message_returns &&
                   !catalog->task_personal_messages)) {
                if (!family_voluntary_paths_enabled()) {
                  valid = validate_player_plan_world(*this);
                  if (!valid.ok)
                    return valid;
                }
              }
            }
          }
        }
      }
    }
  }
  valid = validate_military_tactic_learning_world_structure(*this);
  if (!valid.ok)
    return valid;
  auto field_selection = selected_field_battle_version(catalog->identity);
  if (!field_selection.ok)
    return {false, {}, std::move(field_selection.error)};
  if (field_selection.value && complete_validation) {
    // 后半整世界冷验也必须读同 World 的精确任职/成军来源，不能退回旧 action_facts。
    // 真 owner 与 lease 保活到校验返回；原完整军事校验仍逐项执行，未选保持旧入口。
    auto owner = complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, std::move(owner.error)};
    auto actual = actual_phase_moment();
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    auto lease = owner.value->read_military(actual.value);
    if (!lease.ok)
      return {false, {}, std::move(lease.error)};
    if (!lease.value)
      return common_failure("MilitaryExactReadLeaseRequired");
    const auto binding = lease.value->bindings();
    if (&binding.common != &common || &binding.military != &military ||
        &binding.catalog != catalog->military.get() || !binding.field_supply)
      return common_failure("FieldMilitarySupplyOriginalSourcesRequired");
    valid = validate_military_state(common, military, *catalog->military,
                                     binding.access, *binding.field_supply);
  } else {
    valid = validate_military_state(common, military, *catalog->military, ma);
  }
  if (!valid.ok)
    return valid;
  valid = validate_native_military_battle_lifecycle_sources(*this);
  if (!valid.ok)
    return valid;
  valid = validate_military_supply_attrition_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_military_rout_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_military_command_resources_modules(
      catalog->identity.module_versions);
  if (!valid.ok)
    return valid;
  valid = validate_military_field_terrain_modules(
      catalog->identity.module_versions);
  if (!valid.ok)
    return valid;
  if (std::any_of(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(),
                  [](const auto &m) {
                    return m.first == military_field_terrain_module;
                  }) !=
      bool(catalog->military && catalog->military->field_terrain_policy))
    return common_failure("MilitaryFieldTerrainProfileMismatch");
  if (std::any_of(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(), [](const auto &m) {
                    return m.first == military_combat_conditions_module;
                  }) != military_combat_conditions_enabled())
    return common_failure("MilitaryCombatConditionProfileMismatch");
  if (std::any_of(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(), [](const auto &m) {
                    return m.first == military_commanded_damage_module;
                  }) != military_commanded_damage_enabled())
    return common_failure("MilitaryCommandedDamageProfileMismatch");
  if (std::any_of(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(), [](const auto &m) {
                    return m.first == military_sequential_module;
                  }) != military_sequential_enabled())
    return common_failure("MilitarySequentialProfileMismatch");
  if (std::any_of(catalog->identity.module_versions.begin(),
                  catalog->identity.module_versions.end(), [](const auto &m) {
                    return m.first == military_command_resources_module;
                  }) != military_command_resources_enabled())
    return common_failure("MilitaryCommandResourcesProfileMismatch");
  const bool officers =
      catalog->military->officer_assignment_policy.has_value();
  if (officers != std::any_of(catalog->identity.module_versions.begin(),
                              catalog->identity.module_versions.end(),
                              [](const auto &m) {
                                return m.first == military_officer_module;
                              }) ||
      (officers && !dynamic_enabled()))
    return common_failure("MilitaryOfficerAssignmentProfileMismatch");
  if (!officers)
    for (const auto &h : common.state().history)
      if (h.kind == "military_officer_assign")
        return common_failure("MilitaryOfficerAssignmentProfileRequired",
                              h.result_id);
  // 回执只绑定原军籍应用事实；政治原授权史由同候选S6读口另外严格验证。
  for (const auto &row : common.state().results.rows()) {
    if (!row.second)
      continue;
    const auto &r = *row.second;
    const auto *e =
        r.details ? std::get_if<MilitaryEffect>(&r.details->data) : nullptr;
    const bool assignment =
        e && e->receipt_id.rfind("military_assignment:", 0) == 0;
    if (r.kind != "military_officer_assign" && !assignment)
      continue;
    if (!officers || r.kind != "military_officer_assign" || !e)
      return common_failure("MilitaryOfficerCachedEffectMismatch", row.first);
    StableId actor;
    for (const auto &h : common.state().history)
      if (h.result_id == r.result_id) {
        if (!actor.empty() || h.kind != r.kind)
          return common_failure("MilitaryOfficerCachedEffectMismatch",
                                row.first);
        actor = h.actor_id;
      }
    auto linked = validate_military_officer_cached_effect(military, *e, actor);
    if (!linked.ok)
      return linked;
    const MilitaryOfficerAssignmentFact *assigned = nullptr;
    for (const auto &f : military.officer_assignments)
      if (f.id == e->receipt_id)
        assigned = &f;
    if (!assigned || r.tick < assigned->tick ||
        (!e->already_completed.value_or(false) && r.tick != assigned->tick))
      return common_failure("MilitaryOfficerCachedEffectMismatch", row.first);
  }

  if (naval_enabled()) {
    auto plans =
        validate_naval_fleet_plan_sources(*catalog->naval, *catalog->military,
                                          *catalog->common, *catalog->travel);
    if (!plans.ok)
      return plans;
    valid = naval_gunnery_enabled()
                ? validate_naval_battle_structure_for_gunnery(
                      common, military, *catalog->military, ma, travel, naval,
                      *catalog->naval, naval_history_access(*this))
            : naval_combat_enabled()
                ? validate_naval_battle_history(
                      common, military, *catalog->military, ma, travel, naval,
                      *catalog->naval, naval_history_access(*this))
                : validate_naval_world(common, military, *catalog->military, ma,
                                       naval, *catalog->naval);
    if (!valid.ok)
      return valid;
  }
  if (naval_repairs_enabled()) {
    auto history = validate_naval_command_history(*this);
    if (!history.ok)
      return history;
  }
  const bool recovery = military_recovery_enabled();
  if (recovery != std::any_of(catalog->identity.module_versions.begin(),
                              catalog->identity.module_versions.end(),
                              [](const auto &row) {
                                return row.first == military_recovery_module;
                              }))
    return common_failure("MilitaryRecoveryProfileMismatch");
  if (recovery) {
    valid = validate_military_recovery_structure(
        common, military, *catalog->military, ma, military.recovery,
        *catalog->military->recovery_policy,
        population_military_enabled() ? &population_military : nullptr);
    if (!valid.ok)
      return valid;
    valid = validate_military_recovery_current_rosters(
        common, military, *catalog->military,
        population_military_enabled() ? &population_military : nullptr,
        population_military_enabled() ? catalog->population_military.get()
                                      : nullptr);
    if (!valid.ok)
      return valid;
  }
  valid = validate_military_garrison_rest_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_military_siege_engineering_world_structure(*this);
  if (!valid.ok)
    return valid;
  const bool field_commands =
      catalog->military->field_command_policy.has_value();
  if (field_commands != std::any_of(catalog->identity.module_versions.begin(),
                                    catalog->identity.module_versions.end(),
                                    [](const auto &m) {
                                      return m.first ==
                                             military_field_command_module;
                                    }) ||
      (field_commands && !officers))
    return common_failure("MilitaryFieldCommandProfileMismatch");
  if (!field_commands)
    for (const auto &h : common.state().history)
      if (h.kind == "military_field_command_assign")
        return common_failure("MilitaryFieldCommandProfileRequired",
                              h.result_id);
  for (const auto &row : common.state().results.rows()) {
    if (!row.second)
      continue;
    const auto &r = *row.second;
    const auto *e =
        r.details ? std::get_if<MilitaryEffect>(&r.details->data) : nullptr;
    const bool source =
        e && e->receipt_id.rfind("military_field_command:", 0) == 0;
    if (r.kind != "military_field_command_assign" && !source)
      continue;
    if (!field_commands || r.kind != "military_field_command_assign" || !e)
      return common_failure("MilitaryFieldCommandCachedEffectMismatch",
                            row.first);
    StableId actor;
    for (const auto &h : common.state().history)
      if (h.result_id == r.result_id) {
        if (!actor.empty() || h.kind != r.kind)
          return common_failure("MilitaryFieldCommandCachedEffectMismatch",
                                row.first);
        actor = h.actor_id;
      }
    auto linked =
        validate_military_field_command_cached_effect(military, *e, actor);
    if (!linked.ok)
      return linked;
    const MilitaryFieldCommandFact *assigned = nullptr;
    for (const auto &f : military.field_commands)
      if (f.id == e->receipt_id)
        assigned = &f;
    if (!assigned || r.tick < assigned->tick ||
        (!e->already_completed.value_or(false) && r.tick != assigned->tick))
      return common_failure("MilitaryFieldCommandCachedEffectMismatch",
                            row.first);
  }
  // 战损终止的计划绑定在原军事/舟战历史通过后读精确来源；无新战损profile保原次序。
  if (!growth_tactic_lessons_enabled() && player_plan_enabled() &&
      !catalog->contact_inquiries && !catalog->personal_appointments &&
      !private_shops_enabled() && !personal_recovery_enabled() &&
      !family_birth_enabled() && !naval_gunnery_enabled() &&
      (naval_combat_enabled() || field_commander_losses)) {
    if (!family_disciple_legacy_enabled() &&
        !family_biological_legacy_enabled() &&
        !family_collateral_legacy_enabled()) {
      if (!military_commanded_damage_enabled()) {
        if (!catalog->visit_appointment_invitations) {
          if (!private_shop_command_results_enabled()) {
            if ((!catalog->task_shop_order_contacts_enabled &&
                 !catalog->task_message_returns &&
                 !catalog->task_personal_messages)) {
              if (!family_voluntary_paths_enabled()) {
                valid = validate_player_plan_world(*this);
                if (!valid.ok)
                  return valid;
              }
            }
          }
        }
      }
    }
  }
  if (family_life_enabled() != family_life_seed_complete)
    return common_failure("FamilyLifeProfileMismatch");
  valid = validate_native_control_continuation_world(*this);
  if (!valid.ok)
    return valid;
  if (catalog->family_life && catalog->family_life->selected_version == 4)
    valid = validate_native_life_age_world(*this);
  else
    valid = validate_family_death_coverage(
        common, family_life, family_death_read(), family_life_enabled());
  if (!valid.ok)
    return valid;
  {
    const bool enabled = bool(catalog->task_message_notices);
    Integer markers = 0;
    for (const auto &marker : catalog->identity.module_versions)
      if (marker.first == "task_message_notices") {
        ++markers;
        if (marker.second != "1")
          return common_failure("UnsupportedTaskMessageNoticeVersion");
      }
    if (markers != (enabled ? 1 : 0) ||
        enabled != tasks.message_notices.has_value())
      return common_failure("TaskMessageNoticeProfileMismatch");
    if (!enabled) {
      for (const auto &row : common.state().facts.rows())
        if (row.second.kind == task_death_notice_bound_kind)
          return common_failure("TaskMessageNoticeProfileRequired", row.first);
    } else {
      auto notice = make_task_message_notice_services(*this);
      if (!notice.ok)
        return {false, {}, notice.error};
      if (!notice.value)
        return common_failure("TaskMessageNoticeServicesMismatch");
      valid = validate_task_message_notices(
          common, tasks, *catalog->tasks, *catalog->common,
          *catalog->task_message_notices, *tasks.message_notices,
          *tasks.message_handoffs, notice.value->validate);
      if (!valid.ok)
        return valid;
    }
  }
  // 新回执复用FamilyEffect标签，但不能把未启用域的历史或缓存藏进旧profile。
  auto legacy_command = [&](const StableId &kind) -> Status {
    if (kind.rfind("family_collateral_", 0) == 0) {
      if (!family_collateral_legacy_enabled())
        return common_failure("CollateralControlProfileRequired");
      return kind == "family_collateral_recognize" ||
                     kind == "family_collateral_accept" ||
                     kind == "family_collateral_revoke"
                 ? common_success()
                 : common_failure("UnknownCollateralControlCommand");
    }
    if (kind.rfind("family_biological_", 0) == 0) {
      if (!family_biological_legacy_enabled())
        return common_failure("BiologicalControlProfileRequired");
      return kind == "family_biological_accept" ||
                     kind == "family_biological_revoke"
                 ? common_success()
                 : common_failure("UnknownBiologicalControlCommand");
    }
    if (kind.rfind("family_legacy_", 0) != 0)
      return common_success();
    if (!family_legacy_enabled())
      return common_failure("LegacyControlConsentProfileRequired");
    return kind == "family_legacy_accept" || kind == "family_legacy_revoke" ||
                   kind == "family_legacy_activate"
               ? common_success()
               : common_failure("UnknownFamilyLegacyCommand");
  };
  for (const auto &row : common.state().history) {
    auto r = legacy_command(row.kind);
    if (!r.ok)
      return r;
  }
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      auto r = legacy_command(row.second->kind);
      if (!r.ok)
        return r;
    }
  // 新kind必须带原FamilyEffect标签并精确引用本域事实，旧命令也不能夹带新域事实ID。
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      const auto &result = *row.second;
      const bool biological_kind =
          result.kind.rfind("family_biological_", 0) == 0;
      const bool legacy_kind = result.kind.rfind("family_legacy_", 0) == 0;
      const auto *receipt =
          result.details ? std::get_if<FamilyEffect>(&result.details->data)
                         : nullptr;
      const bool legacy_fact =
          receipt && receipt->fact_id &&
          receipt->fact_id->rfind("family_legacy:", 0) == 0;
      const bool collateral_kind =
          result.kind.rfind("family_collateral_", 0) == 0;
      const bool collateral_fact =
          receipt && receipt->fact_id &&
          receipt->fact_id->rfind("family_collateral:", 0) == 0;
      if (collateral_kind || collateral_fact) {
        if (!family_collateral_legacy_enabled() || !collateral_kind ||
            !receipt ||
            (result.kind == "family_collateral_recognize" ? !collateral_fact
                                                          : !legacy_fact))
          return common_failure("InvalidCollateralControlReceipt", row.first);
        continue;
      }
      if (biological_kind) {
        if (!family_biological_legacy_enabled() || !receipt || !legacy_fact)
          return common_failure("InvalidBiologicalControlReceipt", row.first);
        continue;
      }
      if (!legacy_kind && !legacy_fact)
        continue;
      if (!family_legacy_enabled() || !legacy_kind || !receipt || !legacy_fact)
        return common_failure("InvalidFamilyLegacyReceipt", row.first);
      const HistoryRecord *history = nullptr;
      for (const auto &entry : common.state().history)
        if (entry.result_id == result.result_id) {
          if (history)
            return common_failure("FamilyLegacyCachedHistoryMismatch",
                                  row.first);
          history = &entry;
        }
      if (!history || history->kind != result.kind ||
          history->tick != result.tick)
        return common_failure("FamilyLegacyCachedHistoryMismatch", row.first);
      auto checked = validate_family_legacy_receipt(
          family_legacy, result.kind, history->actor_id, result.tick, *receipt);
      if (!checked.ok)
        return checked;
    }
  if (family_legacy_enabled() != family_legacy_seed_complete ||
      (family_legacy_enabled() && !family_life_enabled()))
    return common_failure("FamilyLegacyProfileMismatch");
  // 弟子许可的关系与人物依赖完整Birth和师承；旧profile保持原早位。
  if (!family_disciple_legacy_enabled() &&
      !family_biological_legacy_enabled() &&
      !family_collateral_legacy_enabled()) {
    valid = validate_family_legacy(
        common, family, family_life, family_death_read(), *catalog->common,
        *catalog->interaction,
        catalog->family_legacy ? *catalog->family_legacy
                               : LegacyControlConsentCatalog{},
        family_legacy, family_legacy_playable_read());
    if (!valid.ok)
      return valid;
  }
  // 独立遗赠profile、历史和精确回执同时约束，不能在旧世界夹入新声明。
  if (family_bequest_enabled() != family_bequest_seed_complete ||
      (family_bequest_enabled() && !family_life_enabled()))
    return common_failure("FamilyBequestProfileMismatch");
  auto bequest_kind = [](const StableId &kind) {
    return kind == "family_bequest_declare" || kind == "family_bequest_revoke";
  };
  for (const auto &entry : common.state().history)
    if (entry.kind.rfind("family_bequest_", 0) == 0 &&
        entry.kind != "family_bequest_disclose" &&
        (!family_bequest_enabled() || !bequest_kind(entry.kind)))
      return common_failure("FamilyBequestProfileRequired");
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      const auto &result = *row.second;
      const bool kind = result.kind.rfind("family_bequest_", 0) == 0 &&
                        result.kind != "family_bequest_disclose";
      const auto *receipt =
          result.details ? std::get_if<FamilyEffect>(&result.details->data)
                         : nullptr;
      const bool source = receipt && receipt->fact_id &&
                          receipt->fact_id->rfind("family_bequest:", 0) == 0;
      if (!kind && !source)
        continue;
      if (!family_bequest_enabled() || !bequest_kind(result.kind) || !receipt ||
          !source)
        return common_failure("InvalidBequestReceipt", row.first);
      const HistoryRecord *history = nullptr;
      for (const auto &entry : common.state().history)
        if (entry.result_id == result.result_id) {
          if (history)
            return common_failure("BequestCachedHistoryMismatch");
          history = &entry;
        }
      if (!history || history->kind != result.kind ||
          history->tick != result.tick)
        return common_failure("BequestCachedHistoryMismatch");
      auto checked = validate_family_bequest_receipt(
          family_bequest, result.kind, history->actor_id, result.tick,
          *receipt);
      if (!checked.ok)
        return checked;
    }
  if (!family_voluntary_paths_enabled()) {
    valid = validate_family_bequests(
        common, family, family_life, family_death_read(),
        catalog->family_bequest ? *catalog->family_bequest
                                : FamilyBequestPolicy{},
        family_bequest, family_bequest_player_read());
    if (!valid.ok)
      return valid;
  }
  // 接受和交割另有原通知/原权限来源；未知新前缀与旧profile夹入本域账都拒绝。
  if (family_inheritance_enabled() != family_inheritance_seed_complete ||
      (family_inheritance_enabled() &&
       (!family_bequest_enabled() || !catalog->task_message_notices)))
    return common_failure("FamilyInheritanceProfileMismatch");
  auto inheritance_kind = [](const StableId &kind) {
    return kind == "family_bequest_disclose" ||
           kind == "family_inheritance_accept" ||
           kind == "family_inheritance_settle";
  };
  for (const auto &entry : common.state().history)
    if ((entry.kind == "family_bequest_disclose" ||
         entry.kind.rfind("family_inheritance_", 0) == 0) &&
        (!family_inheritance_enabled() || !inheritance_kind(entry.kind)))
      return common_failure("FamilyInheritanceProfileRequired");
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      const auto &result = *row.second;
      const bool kind = result.kind == "family_bequest_disclose" ||
                        result.kind.rfind("family_inheritance_", 0) == 0;
      const auto *receipt =
          result.details ? std::get_if<FamilyEffect>(&result.details->data)
                         : nullptr;
      const bool source =
          receipt && receipt->fact_id &&
          receipt->fact_id->rfind("family_inheritance:", 0) == 0;
      if (!kind && !source)
        continue;
      if (!family_inheritance_enabled() || !inheritance_kind(result.kind) ||
          !receipt || !source)
        return common_failure("InvalidInheritanceReceipt", row.first);
      const HistoryRecord *history = nullptr;
      for (const auto &entry : common.state().history)
        if (entry.result_id == result.result_id) {
          if (history)
            return common_failure("InheritanceCachedHistoryMismatch");
          history = &entry;
        }
      if (!history || history->kind != result.kind ||
          history->tick != result.tick)
        return common_failure("InheritanceCachedHistoryMismatch");
      auto checked = validate_family_inheritance_receipt(
          family_bequest, family_inheritance, result.kind, history->actor_id,
          result.tick, *receipt);
      if (!checked.ok)
        return checked;
    }
  valid = validate_siege_integration_world(*this);
  if (!valid.ok)
    return valid;
  if (!siege_integration_enabled())
    for (const auto &row : common.state().results.rows())
      if (row.second && siege_integration_command_kind(row.second->kind))
        return common_failure("SiegeIntegrationCapabilityRequired", row.first);
  if (dynamic_enabled()) {
    valid = validate_dynamic_provision_owners(*this, common.tick());
    if (!valid.ok)
      return valid;
    auto authority = services.organization_access();
    std::unique_ptr<CompleteExactWorldBindingOwner> population_owner;
    std::optional<PopulationMilitaryReadContext> population_context;
    if (population_military_enabled()) {
      auto selected = selected_population_military_version(
          *catalog->population_military, siege_integration_enabled());
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value == 4) {
        auto owner = complete_exact_time_bindings();
        if (!owner.ok)
          return {false, {}, owner.error};
        if (!owner.value || !owner.value->reads().precise_population)
          return common_failure("PopulationMilitaryExactOwningSourcesRequired");
        population_owner = std::move(owner.value);
        const auto &actual = *population_owner->reads().precise_population;
        if (&actual.common != &common || &actual.economy != &local_economy ||
            &actual.military != &military ||
            &actual.state != &population_military ||
            &actual.catalog != catalog->population_military.get() ||
            actual.selected_version != 4 || !military_exact ||
            actual.military_exact != &*military_exact || !actual.actual_clock ||
            !actual.dynamic_land_sources ||
            actual.actual_integration != siege_integration_enabled() ||
            actual.population_transport !=
                (population_transport ? &*population_transport : nullptr))
          return common_failure("PopulationMilitaryExactCandidateMismatch");
        population_context.emplace(actual);
      } else {
        population_context.emplace(population_read());
        population_context->selected_version = selected.value;
        population_context->actual_integration = siege_integration_enabled();
      }
      valid = validate_population_military(*population_context);
      if (!valid.ok)
        return valid;
    }
    if (takeover_access) {
      valid = validate_political_takeover_state(takeover_access->context());
      if (!valid.ok)
        return valid;
    }
    std::optional<EstateEmploymentWorldAccess> employment;
    if (estate_employment_enabled())
      employment.emplace(*this);
    std::optional<PrivateShopResponsibilitySourceContext> private_obligations;
    if (private_shops_enabled())
      private_obligations.emplace(PrivateShopResponsibilitySourceContext{
          common, local_economy, *catalog->local_economy, private_shops,
          *catalog->private_shops, nullptr,
          catalog->private_shop_daily_rent.get(),
          private_shop_daily_rent_enabled() ? &private_shop_daily_rent
                                            : nullptr});
    std::unique_ptr<NativeProductionCyclesRead> production_owner;
    std::optional<EconomicExactReadBinding> production_binding;
    if (catalog->production_cycles) {
      auto read = NativeProductionCyclesRead::create(*this);
      if (!read.ok)
        return {false, {}, std::move(read.error)};
      production_owner = std::move(read.value);
      production_binding.emplace(production_owner->bindings());
    }
    valid = validate_local_economy(
        common, local_economy, *catalog->local_economy, authority,
        population_context ? &*population_context : nullptr,
        takeover_economy ? &*takeover_economy : nullptr,
        takeover_economy ? &economic_takeover_execution : nullptr,
        employment ? &employment->context() : nullptr,
        private_obligations ? &*private_obligations : nullptr,
        production_binding ? std::nullopt : unselected_clock,
        production_binding ? false : exact_supply_source_scope,
        production_binding ? &production_binding->original : nullptr);
    if (!valid.ok)
      return valid;
    // 军事稳定态已核占领凭据与精确资产清单；旧grant只能沿该链保留原公库引用。
    auto resource_reads =
        make_native_organization_resource_validation_reads(*this, ma);
    // Complete正文已在本次validate_exact_time_world经真office/clock全验，不能再借无来源旧门。
    const bool complete_politics_body =
        catalog->exact_time && exact_complete_selected(*catalog->exact_time);
    if (!complete_politics_body) {
      valid = validate_organization(
          common, organization, *catalog->organization, local_economy,
          &services, &services, resource_reads.resource_history,
          resource_reads.prospective_grants);
      if (!valid.ok)
        return valid;
    }
    // 新域共用Effects只按精确已启命令交末位本域门；旧五命令与未知种类仍保原拒绝。
    if (politics_enabled()) {
      const PoliticsReceiptReader original_receipts =
          [this](const CommitResult &r) {
            return politics_receipt_for_original(
                r, {bool(catalog->political_successions) &&
                        bool(political_successions),
                    bool(catalog->political_head_handovers) &&
                        bool(political_head_handovers),
                    bool(catalog->political_successor_rounds) &&
                        bool(political_successor_rounds),
                    bool(catalog->political_term_handovers) &&
                        bool(political_term_handovers),
                    bool(catalog->political_office_receptions) &&
                        bool(political_office_receptions),
                    bool(catalog->political_office_resignations) &&
                        bool(political_office_resignations),
                    bool(catalog->political_governance_agencies) &&
                        bool(political_governance_agencies),
                    bool(catalog->political_subordinate_appointments) &&
                        bool(political_subordinate_appointments)});
          };
      valid = complete_politics_body
                  ? validate_political_business_receipts(politics_read(), true,
                                                         original_receipts)
                  : validate_political_business(
                        politics_read(), true, original_receipts,
                        takeover_access ? &takeover_access->territories()
                                        : nullptr);
      if (!valid.ok)
        return valid;
    }
    if (political_fiscal_enabled() && takeover_access) {
      valid = validate_political_fiscal(
          common, *catalog->common, politics, political_fiscal,
          *catalog->political_fiscal, organization, *catalog->organization,
          &takeover_access->fiscal());
      if (!valid.ok)
        return valid;
    }
    if (political_fiscal_enabled()) {
      const PoliticalFiscalReceiptReader receipts = [](const CommitResult &r) {
        return r.kind == "political_set_tax" && r.details
                   ? std::get_if<PoliticalFiscalEffects>(&r.details->data)
                   : nullptr;
      };
      valid = selected_fiscal2 ? validate_political_fiscal_business_receipts(
                                     political_fiscal_read(), true, receipts)
                               : validate_political_fiscal_business(
                                     political_fiscal_read(), true, receipts);
      if (!valid.ok)
        return valid;
    }
    if (politics_governance_enabled() &&
        !catalog->political_governance_agencies) {
      std::unique_ptr<OccupationFiscalSettlementWorldAccess> occupation_civil;
      if (occupation_fiscal_enabled()) {
        auto access = OccupationFiscalSettlementWorldAccess::create(*this);
        if (!access.ok)
          return {false, {}, access.error};
        occupation_civil = std::move(access.value);
      }
      GovernanceTaskRead governance_read(common, tasks, governance_tasks);
      PublicReliefRead relief_read(common, public_relief);
      valid = validate_political_governance(
          common, *catalog->common, politics, politics_governance,
          *catalog->politics_governance, organization, *catalog->organization,
          services, governance_read, relief_read,
          political_fiscal_enabled() ? &political_fiscal : nullptr,
          fiscal_settlement_enabled() ? &fiscal_settlement : nullptr,
          occupation_civil.get());
      if (!valid.ok)
        return valid;
    }
    valid = validate_economic_configuration_receipts(*this);
    if (!valid.ok)
      return valid;
    valid = validate_household_provision_configuration_receipts(*this);
    if (!valid.ok)
      return valid;
    valid = validate_treaty_passage_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_political_military_appointment_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_political_field_appointment_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_occupation_fiscal_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_occupation_fiscal_need_layout(
        catalog->occupation_fiscal_needs_enabled,
        catalog->identity.module_versions);
    if (!valid.ok)
      return valid;
    valid = validate_economic_fiscal_world(*this);
    if (!valid.ok)
      return valid;
    if (public_relief_enabled() && !catalog->political_governance_agencies) {
      if (!catalog->politics || !catalog->politics_governance)
        return common_failure("PublicReliefGovernanceDependencyRequired");
      valid = validate_public_relief_catalog_links(
          common, local_economy, *catalog->public_relief, *catalog->politics,
          *catalog->politics_governance);
      if (!valid.ok)
        return valid;
      GovernanceTaskRead task_sources(common, tasks, governance_tasks);
      PoliticsGovernanceAccess authority(
          common, politics, politics_governance, *catalog->politics_governance,
          organization, *catalog->organization, services,
          catalog->political_governance_agencies ? &services : nullptr);
      valid = validate_public_relief(
          common, local_economy, *catalog->public_relief, public_relief,
          task_sources, authority,
          takeover_economy ? &*takeover_economy : nullptr,
          takeover_economy ? &economic_takeover_execution : nullptr);
      if (!valid.ok)
        return valid;
    }
    valid = validate_named_membership_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_economic_supply_world(*this);
    if (!valid.ok)
      return valid;
    // 新欠款映射在首期到期前也须指向真实工资/付款义务；这里只核来源，不补造义务或余额。
    if (catalog->affairs->obligation_needs)
      for (const auto &row : catalog->affairs->obligation_policies.rows()) {
        const auto &policy = row.second;
        const auto *obligation =
            local_economy.obligations.find(policy.obligation_id);
        if (!obligation ||
            obligation->principal.kind != SubjectKind::organization ||
            obligation->principal.id != policy.organization_id ||
            obligation->beneficiary.kind != SubjectKind::actor ||
            !common.actor(obligation->beneficiary.id) ||
            (obligation->kind != ObligationKind::wage &&
             obligation->kind != ObligationKind::contract_payment))
          return common_failure("InvalidObligationNeedPolicySource", row.first);
      }
    std::optional<FiscalNeedAccess> fiscal_need_read;
    if (catalog->affairs->fiscal_need_policies) {
      if (!fiscal_settlement_enabled())
        return common_failure("FiscalNeedsRequireSettlement");
      if (occupation_fiscal_needs_enabled())
        fiscal_need_read.emplace(local_economy, fiscal_settlement,
                                 *catalog->fiscal_settlement,
                                 *catalog->affairs->fiscal_need_policies,
                                 occupation_fiscal_settlement);
      else
        fiscal_need_read.emplace(local_economy, fiscal_settlement,
                                 *catalog->fiscal_settlement,
                                 *catalog->affairs->fiscal_need_policies);
    }
    std::optional<CohortWageNeedAccess> cohort_wage_read;
    if (catalog->affairs->cohort_wage_need_policies)
      cohort_wage_read.emplace(local_economy, *catalog->local_economy,
                               *catalog->affairs->cohort_wage_need_policies,
                               takeover_economy ? &*takeover_economy : nullptr,
                               takeover_economy ? &economic_takeover_execution
                                                : nullptr);
    auto supply_suspensions =
        takeover_economy
            ? read_economic_supply_suspensions(*takeover_economy)
            : Outcome<std::vector<EconomicSupplySuspension>>{true, {}, {}};
    if (!supply_suspensions.ok)
      return {false, {}, supply_suspensions.error};
    std::unique_ptr<EconomicSupplyWorldAccess> issued_supply;
    if (economic_supply_policies_enabled()) {
      auto access = EconomicSupplyWorldAccess::create(*this);
      if (!access.ok)
        return {false, {}, access.error};
      issued_supply = std::move(access.value);
    }
    valid = issued_supply
                ? validate_affairs_with_issued_supply(
                      common, affairs, *catalog->affairs, tasks,
                      *catalog->tasks, issued_supply->read(), &services,
                      fiscal_need_read ? &*fiscal_need_read : nullptr,
                      cohort_wage_read ? &*cohort_wage_read : nullptr,
                      takeover_economy ? &supply_suspensions.value : nullptr)
                : validate_affairs(
                      common, affairs, *catalog->affairs, tasks, &services,
                      fiscal_need_read ? &*fiscal_need_read : nullptr,
                      cohort_wage_read ? &*cohort_wage_read : nullptr,
                      takeover_economy ? &supply_suspensions.value : nullptr);
    if (!valid.ok)
      return valid;
    WorldCurrentMomentRead family_clock(*this);
    valid = validate_dynamic_tasks(
        common, tasks, *catalog->tasks, *catalog->common,
        takeover_responsibilities,
        catalog->exact_time && exact_complete_selected(*catalog->exact_time)
            ? &family_clock
            : nullptr);
    if (!valid.ok)
      return valid;
    valid = validate_organization_contract_authority(
        common, organization, *catalog->organization, affairs, tasks);
    if (!valid.ok)
      return valid;
    TaskCargoAccess cargo(common, personal, *catalog->personal, interaction);
    valid = validate_task_cargo_sources(common, tasks, cargo);
    if (!valid.ok)
      return valid;
    if (politics_enabled()) {
      valid = validate_political_director(common, politics, *catalog->politics,
                                          organization, director,
                                          *catalog->director);
      if (!valid.ok)
        return valid;
    } else if (catalog->director->politics)
      return common_failure("PoliticalDirectorProfileMismatch");
    valid = validate_treaty_passage_director(
        *catalog->director, director, catalog->treaty_passage.get(),
        treaty_passage_enabled() ? &treaty_passage : nullptr,
        *catalog->organization);
    if (!valid.ok)
      return valid;
    valid = validate_political_supply_renewal_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_diplomatic_director_catalog(
        *catalog->director, *catalog->organization, catalog->diplomacy.get(),
        catalog->identity.module_versions);
    if (!valid.ok)
      return valid;
    valid = validate_diplomatic_director_history(
        director, *catalog->director,
        diplomacy_enabled() ? &diplomacy : nullptr);
    if (!valid.ok)
      return valid;
    valid = validate_diplomatic_delegation_director_catalog(
        *catalog->director, *catalog->organization, catalog->diplomacy.get());
    if (!valid.ok)
      return valid;
    valid = validate_diplomatic_delegation_director_history(
        director, *catalog->director,
        diplomacy_enabled() ? &diplomacy : nullptr);
    if (!valid.ok)
      return valid;
    // 完整主体已由上面的唯一 exact validator 借真实 owner 验全源；旧布局仍走原
    // registry 门。
    if (!catalog->director->subjects2) {
      valid = validate_director(common, director, *catalog->director,
                                regional_life_enabled() ? &services : nullptr,
                                family.founder_actor_id);
      if (!valid.ok)
        return valid;
    }
    {
      Integer markers = 0;
      for (const auto &marker : catalog->identity.module_versions)
        if (marker.first == "authored_task_continuation") {
          ++markers;
          if (!catalog->director->authored_task_continuation ||
              marker.second !=
                  std::to_string(
                      catalog->director->authored_task_continuation->version))
            return common_failure("UnsupportedAuthoredTaskContinuationVersion");
        }
      if (markers != (catalog->director->authored_task_continuation ? 1 : 0))
        return common_failure("TaskContinuationProfileMismatch");
      if (catalog->director->authored_task_continuation) {
        valid = validate_task_authored_continuation_catalog(
            *catalog->director->authored_task_continuation, *catalog->tasks,
            *catalog->common);
        if (!valid.ok)
          return valid;
      }
    }
    if (!(catalog->exact_time &&
          (exact_social_selected(*catalog->exact_time) ||
           exact_complete_selected(*catalog->exact_time)))) {
      valid = validate_dynamic_social(common, social, &services,
                                      named_membership_enabled());
      if (!valid.ok)
        return valid;
    }
    valid = validate_regional_world(*this);
    if (!valid.ok)
      return valid;
  }
  // 私产历史免责只读依赖经济、财政、赈济和Task原历史，必须在上述全部原域验证之后消费。
  {
    auto layout = validate_task_static_asset_layout(
        common, catalog->task_static_assets_enabled,
        catalog->identity.module_versions,
        tasks.static_assets ? &*tasks.static_assets : nullptr);
    if (!layout.ok)
      return layout;
    if (tasks.static_assets) {
      if (!dynamic_enabled())
        return common_failure("TaskStaticAssetProfileMismatch");
      valid =
          validate_task_static_assets(common, tasks, *catalog->tasks,
                                      *catalog->common, *tasks.static_assets);
      if (!valid.ok)
        return valid;
      valid = validate_task_static_asset_cash_sources(common, tasks,
                                                      *tasks.static_assets);
      if (!valid.ok)
        return valid;
    }
  }
  valid = validate_task_extension_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_message_return_layout(
      common, catalog->identity.module_versions,
      tasks.message_returns ? &*tasks.message_returns : nullptr);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_message_layout(
      common, catalog->identity.module_versions,
      tasks.personal_messages ? &*tasks.personal_messages : nullptr);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_report_layout(
      common, catalog->identity.module_versions,
      tasks.personal_reports ? &*tasks.personal_reports : nullptr);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_acceptance_layout(
      common, catalog->identity.module_versions,
      tasks.personal_acceptances ? &*tasks.personal_acceptances : nullptr);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_cancellation_layout(
      common, catalog->identity.module_versions,
      tasks.personal_cancellations ? &*tasks.personal_cancellations : nullptr);
  if (!valid.ok)
    return valid;
  // 私人免责消费就业权责前，先验原经济/任务来源及同候选就业全史。
  if (!dynamic_enabled()) {
    valid = validate_economic_supply_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_political_supply_renewal_world(*this);
    if (!valid.ok)
      return valid;
  }
  if (!family_birth_enabled()) {
    valid = validate_estate_employment_world(*this);
    if (!valid.ok)
      return valid;
  }
  valid = validate_family_birth_world(*this);
  if (!valid.ok)
    return valid;
  // 原出生/生命全验后，先核打开控制链与真实结果；Legacy2授权后继消费本来源。
  valid = validate_born_personal_dialogue_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_known_personal_dialogue_world(*this);
  if (!valid.ok)
    return valid;
  // 新生人物真实出生全史通过后才授旧邀约/接受的成年资格；早位公责仅消费具名原账来源。
  if (family_birth_enabled()) {
    valid = validate_estate_employment_world(*this);
    if (!valid.ok)
      return valid;
  }
  // 原创建/出生与军事死因已完整验证，再核预约本人来源和实际缓存；不以今日alive倒推旧承诺。
  valid = validate_personal_appointment_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_contact_inquiry_world(*this);
  if (!valid.ok)
    return valid;
  if (!catalog->visit_message_replies ||
      catalog->visit_message_replies->version == 1) {
    valid = validate_visit_reply_world(*this);
    if (!valid.ok)
      return valid;
  }
  valid = validate_personal_appointment_attendance_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_child_care_relief_world(*this);
  if (!valid.ok)
    return valid;
  // 显式先天课程只有完整出生、谱系、照护与原死亡来源已通过后才读取旧开工禀赋。
  if (catalog->growth->innate_learning) {
    if (!family_birth_enabled())
      return common_failure("InnateLearningProfileRequired");
    valid = validate_selected_growth_state(*this);
    if (!valid.ok)
      return valid;
  }
  // Naval6早段只验机械/原事实结构；同候选Family、Birth、Growth全部通过后必补XP来源。
  if (naval_gunnery_enabled()) {
    valid = validate_naval_gunnery_world(*this);
    if (!valid.ok)
      return valid;
    if (!growth_tactic_lessons_enabled() && player_plan_enabled() &&
        !catalog->contact_inquiries && !catalog->personal_appointments &&
        !private_shops_enabled() && !personal_recovery_enabled() &&
        !family_birth_enabled()) {
      if (!family_disciple_legacy_enabled() &&
          !family_biological_legacy_enabled() &&
          !family_collateral_legacy_enabled()) {
        if (!military_commanded_damage_enabled()) {
          if (!catalog->visit_appointment_invitations) {
            if (!private_shop_command_results_enabled()) {
              if ((!catalog->task_shop_order_contacts_enabled &&
                   !catalog->task_message_returns &&
                   !catalog->task_personal_messages)) {
                if (!family_voluntary_paths_enabled()) {
                  valid = validate_player_plan_world(*this);
                  if (!valid.ok)
                    return valid;
                }
              }
            }
          }
        }
      }
    }
  }
  // Task原资产与最终Growth已经通过；随后只读永久市场、法律、私铺和店单，禁止回早位资格。
  // 原家庭/出生及Growth全部历史先于伤因与实际服务逐期康复，军队回队晚门再消费本来源。
  if (military_commanded_damage_enabled()) {
    valid = validate_military_commanded_damage_world(*this);
    if (!valid.ok)
      return valid;
  }
  if (military_command_resources_enabled()) {
    valid = validate_military_command_resources_world(*this);
    if (!valid.ok)
      return valid;
  }
  valid = validate_growth_recovery_world(*this);
  if (!valid.ok)
    return valid;
  if (military_recovery_enabled()) {
    valid = validate_military_wounded_recovery_world_sources(*this);
    if (!valid.ok)
      return valid;
  }
  if (!growth_tactic_lessons_enabled() && player_plan_enabled() &&
      !catalog->contact_inquiries && !private_shops_enabled() &&
      family_birth_enabled()) {
    if (!family_disciple_legacy_enabled() &&
        !family_biological_legacy_enabled() &&
        !family_collateral_legacy_enabled()) {
      if (!military_commanded_damage_enabled()) {
        if (!catalog->visit_appointment_invitations) {
          if (!private_shop_command_results_enabled()) {
            if ((!catalog->task_shop_order_contacts_enabled &&
                 !catalog->task_message_returns &&
                 !catalog->task_personal_messages)) {
              if (!family_voluntary_paths_enabled()) {
                valid = validate_player_plan_world(*this);
                if (!valid.ok)
                  return valid;
              }
            }
          }
        }
      }
    }
  }
  if (!growth_tactic_lessons_enabled() && player_plan_enabled() &&
      !catalog->contact_inquiries && !private_shops_enabled() &&
      personal_recovery_enabled() && !family_birth_enabled()) {
    if (!family_disciple_legacy_enabled() &&
        !family_biological_legacy_enabled() &&
        !family_collateral_legacy_enabled()) {
      if (!military_commanded_damage_enabled()) {
        if (!catalog->visit_appointment_invitations) {
          if (!private_shop_command_results_enabled()) {
            if ((!catalog->task_shop_order_contacts_enabled &&
                 !catalog->task_message_returns &&
                 !catalog->task_personal_messages)) {
              if (!family_voluntary_paths_enabled()) {
                valid = validate_player_plan_world(*this);
                if (!valid.ok)
                  return valid;
              }
            }
          }
        }
      }
    }
  }
  valid = validate_growth_market_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_shop_registry_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_shop_registry_director_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_shop_licensing_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_shop_order_contact_layout(
      common, catalog->identity.module_versions,
      catalog->task_shop_order_contacts_enabled,
      tasks.shop_orders && tasks.shop_orders->contacts
          ? &*tasks.shop_orders->contacts
          : nullptr);
  if (!valid.ok)
    return valid;
  if ((!catalog->task_shop_order_contacts_enabled &&
       !catalog->task_message_returns && !catalog->task_personal_messages)) {
    valid = validate_private_shop_world(*this, projection);
    if (!valid.ok)
      return valid;
    valid = validate_task_shop_orders_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_private_shop_liquidation_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_private_shop_daily_rent_world(*this);
    if (!valid.ok)
      return valid;
  }
  if (!growth_tactic_lessons_enabled() && private_shops_enabled() &&
      player_plan_enabled() && !catalog->contact_inquiries) {
    if (!family_disciple_legacy_enabled() &&
        !family_biological_legacy_enabled() &&
        !family_collateral_legacy_enabled()) {
      if (!military_commanded_damage_enabled()) {
        if (!catalog->visit_appointment_invitations) {
          if (!private_shop_command_results_enabled()) {
            if ((!catalog->task_shop_order_contacts_enabled &&
                 !catalog->task_message_returns &&
                 !catalog->task_personal_messages)) {
              if (!family_voluntary_paths_enabled()) {
                valid = validate_player_plan_world(*this);
                if (!valid.ok)
                  return valid;
              }
            }
          }
        }
      }
    }
  }
  // 未选其他晚序域的会面Continue也须在双方实际存在/生死与预约全史之后消费。
  if (!growth_tactic_lessons_enabled() && player_plan_enabled() &&
      !catalog->contact_inquiries && catalog->personal_appointments &&
      !private_shops_enabled() && !personal_recovery_enabled() &&
      !family_birth_enabled()) {
    if (!family_disciple_legacy_enabled() &&
        !family_biological_legacy_enabled() &&
        !family_collateral_legacy_enabled()) {
      if (!military_commanded_damage_enabled()) {
        if (!catalog->visit_appointment_invitations) {
          if (!private_shop_command_results_enabled()) {
            if ((!catalog->task_shop_order_contacts_enabled &&
                 !catalog->task_message_returns &&
                 !catalog->task_personal_messages)) {
              if (!family_voluntary_paths_enabled()) {
                valid = validate_player_plan_world(*this);
                if (!valid.ok)
                  return valid;
              }
            }
          }
        }
      }
    }
  }
  if (!growth_tactic_lessons_enabled() && player_plan_enabled() &&
      catalog->contact_inquiries) {
    if (!family_disciple_legacy_enabled() &&
        !family_biological_legacy_enabled() &&
        !family_collateral_legacy_enabled()) {
      if (!military_commanded_damage_enabled()) {
        if (!catalog->visit_appointment_invitations) {
          if (!private_shop_command_results_enabled()) {
            if ((!catalog->task_shop_order_contacts_enabled &&
                 !catalog->task_message_returns &&
                 !catalog->task_personal_messages)) {
              if (!family_voluntary_paths_enabled()) {
                valid = validate_player_plan_world(*this);
                if (!valid.ok)
                  return valid;
              }
            }
          }
        }
      }
    }
  }
  if (!family_voluntary_paths_enabled() && !growth_tactic_lessons_enabled() &&
      !family_collateral_legacy_enabled() &&
      (!catalog->task_shop_order_contacts_enabled &&
       !catalog->task_message_returns && !catalog->task_personal_messages)) {
    auto inheritance = family_inheritance_access();
    valid = validate_family_inheritance(
        common, family_bequest, family_life, *catalog->interaction,
        inheritance.player, inheritance.received_notice,
        catalog->family_inheritance ? *catalog->family_inheritance
                                    : FamilyInheritancePolicy{},
        family_inheritance, inheritance.clearance);
    if (!valid.ok)
      return valid;
  }
  // 独立婚议只允许精确本人命令和本域回执，未启用的旧档不得藏入新事实。
  if (family_marriage_enabled() != family_marriage_seed_complete ||
      (family_marriage_enabled() &&
       (!family_life_enabled() || !catalog->family->enabled)))
    return common_failure("FamilyMarriageProfileMismatch");
  auto marriage_kind = [](const StableId &kind) {
    return kind == "family_marriage_receive" ||
           kind == "family_marriage_reply" ||
           kind == "family_marriage_dissolve";
  };
  for (const auto &entry : common.state().history)
    if (entry.kind.rfind("family_marriage_", 0) == 0 &&
        (!family_marriage_enabled() || !marriage_kind(entry.kind)))
      return common_failure("MarriageProfileRequired");
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      const auto &result = *row.second;
      const bool kind = result.kind.rfind("family_marriage_", 0) == 0;
      const auto *receipt =
          result.details ? std::get_if<FamilyEffect>(&result.details->data)
                         : nullptr;
      const bool source = receipt && receipt->fact_id &&
                          receipt->fact_id->rfind("family_marriage:", 0) == 0;
      if (!kind && !source)
        continue;
      if (!family_marriage_enabled() || !marriage_kind(result.kind) ||
          !receipt || !source)
        return common_failure("InvalidMarriageReceipt", row.first);
      const HistoryRecord *history = nullptr;
      for (const auto &entry : common.state().history)
        if (entry.result_id == result.result_id) {
          if (history)
            return common_failure("MarriageCachedHistoryMismatch");
          history = &entry;
        }
      if (!history || history->kind != result.kind ||
          history->tick != result.tick)
        return common_failure("MarriageCachedHistoryMismatch");
      auto checked = validate_family_marriage_receipt(
          family_marriage, result.kind, history->actor_id, result.tick,
          *receipt);
      if (!checked.ok)
        return checked;
    }
  auto marriage = validate_family_marriage(
      common, family, family_life, *catalog->interaction,
      catalog->family_marriage ? *catalog->family_marriage
                               : MarriageSourceCatalog{},
      family_marriage, family_marriage_access());
  if (!marriage.ok)
    return {false, {}, marriage.error};
  // 原军事、人物创建/出生、生命、控制及完整课程都已通过，再核独立师承与永久实际请求。
  valid = validate_family_apprenticeship_world(*this);
  if (!valid.ok)
    return valid;
  // 真实Known打开和完整身份/出生/死亡已验；先认可，才可被原混合控制账消费。
  valid = validate_family_collateral_recognition_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_family_disciple_legacy_world(*this);
  if (!valid.ok)
    return valid;
  // 出生会面已核真实控制前缀，再完整验证唯一接续账和实际结果绑定。
  valid = validate_family_biological_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_family_collateral_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_family_voluntary_paths_world(*this);
  if (!valid.ok)
    return valid;
  if (family_voluntary_paths_enabled()) {
    valid = validate_family_bequests(
        common, family, family_life, family_death_read(),
        catalog->family_bequest ? *catalog->family_bequest
                                : FamilyBequestPolicy{},
        family_bequest, family_bequest_player_read());
    if (!valid.ok)
      return valid;
  }
  // 代理历史本人仅在完整原控制链后读；原早Task结构不变，这里必核原批准、治理Task与全部实付。
  valid = validate_political_governance_agency_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_agency_governance_dependencies_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_message_return_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_acceptance_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_message_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_report_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_task_personal_cancellation_world(*this);
  if (!valid.ok)
    return valid;
  // 新接触来源先完整核历史双方生命/真实控制，再交成本、私铺与原Task完整消费者。
  if (catalog->task_shop_order_contacts_enabled ||
      catalog->task_message_returns || catalog->task_personal_messages) {
    valid = validate_task_shop_order_contact_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_private_shop_world(*this, projection);
    if (!valid.ok)
      return valid;
    valid = validate_task_shop_orders_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_private_shop_liquidation_world(*this);
    if (!valid.ok)
      return valid;
    valid = validate_private_shop_daily_rent_world(*this);
    if (!valid.ok)
      return valid;
  }
  if ((family_collateral_legacy_enabled() ||
       family_voluntary_paths_enabled()) &&
      !growth_tactic_lessons_enabled() &&
      (!catalog->task_shop_order_contacts_enabled &&
       !catalog->task_message_returns && !catalog->task_personal_messages)) {
    auto inheritance = family_inheritance_access();
    valid = validate_family_inheritance(
        common, family_bequest, family_life, *catalog->interaction,
        inheritance.player, inheritance.received_notice,
        catalog->family_inheritance ? *catalog->family_inheritance
                                    : FamilyInheritancePolicy{},
        family_inheritance, inheritance.clearance);
    if (!valid.ok)
      return valid;
  }
  valid = validate_visit_message_receipt_world(*this);
  if (!valid.ok)
    return valid;
  // 原人物/全部控制链、原预约及拜帖先验；本域三源/会面/实际结果后最后合并承诺冲突。
  valid = validate_visit_appointment_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_visit_appointment_attendance_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_private_shop_daily_rent_player_history_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_private_shop_command_results_world(*this, projection);
  if (!valid.ok)
    return valid;
  valid = validate_private_shop_command_result_player_world(*this, projection);
  if (!valid.ok)
    return valid;
  // 回复2真实producer要借原全部控制史，必须在弟子Legacy全验后消费。
  if (catalog->visit_message_replies &&
      catalog->visit_message_replies->version == 2) {
    valid = validate_visit_reply_world(*this);
    if (!valid.ok)
      return valid;
  }
  // 早Originals仅验证具名私约结构；所有经营/法律/任务历史通过后重新核完整政治接管责任。
  if (private_shops_enabled() && takeover_responsibilities) {
    const auto &actual_takeover = selected_takeover2
                                      ? *political_exact_binding->takeover
                                      : takeover_access->context();
    valid = validate_political_takeover_state(actual_takeover);
    if (!valid.ok)
      return valid;
  }
  // 原军事、治理工作与付款全史先验后，再核精确续办及永久玩家结果。
  valid = validate_political_governance_continuation_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_governance_invitation_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_succession_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_head_handover_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_successor_round_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_term_handover_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_office_reception_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_office_resignation_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_subordinate_appointment_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_dismissal_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_subordinate_appointment_director_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_office_resignation_response_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_governance_director_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_political_governance_agency_director_world(*this);
  if (!valid.ok)
    return valid;
  if ((family_disciple_legacy_enabled() || family_biological_legacy_enabled() ||
       family_collateral_legacy_enabled()) &&
      player_plan_enabled() && !growth_tactic_lessons_enabled()) {
    if (!military_commanded_damage_enabled()) {
      if (!catalog->visit_appointment_invitations) {
        if (!private_shop_command_results_enabled()) {
          if ((!catalog->task_shop_order_contacts_enabled &&
               !catalog->task_message_returns &&
               !catalog->task_personal_messages)) {
            if (!family_voluntary_paths_enabled()) {
              valid = validate_player_plan_world(*this);
              if (!valid.ok)
                return valid;
            }
          }
        }
      }
    }
  }
  if (military_commanded_damage_enabled() && player_plan_enabled() &&
      !growth_tactic_lessons_enabled()) {
    if (!catalog->visit_appointment_invitations) {
      if (!private_shop_command_results_enabled()) {
        if ((!catalog->task_shop_order_contacts_enabled &&
             !catalog->task_message_returns &&
             !catalog->task_personal_messages)) {
          if (!family_voluntary_paths_enabled()) {
            valid = validate_player_plan_world(*this);
            if (!valid.ok)
              return valid;
          }
        }
      }
    }
  }
  if (catalog->visit_appointment_invitations && player_plan_enabled() &&
      !growth_tactic_lessons_enabled()) {
    if (!private_shop_command_results_enabled()) {
      if ((!catalog->task_shop_order_contacts_enabled &&
           !catalog->task_message_returns &&
           !catalog->task_personal_messages)) {
        if (!family_voluntary_paths_enabled()) {
          valid = validate_player_plan_world(*this);
          if (!valid.ok)
            return valid;
        }
      }
    }
  }
  if (private_shop_command_results_enabled() && player_plan_enabled() &&
      !growth_tactic_lessons_enabled()) {
    if ((!catalog->task_shop_order_contacts_enabled &&
         !catalog->task_message_returns && !catalog->task_personal_messages)) {
      if (!family_voluntary_paths_enabled()) {
        valid = validate_player_plan_world(*this);
        if (!valid.ok)
          return valid;
      }
    }
  }
  // 完整控制/出生/成长已验，教学完整来源再交军事晚习得，最后继产与Plan。
  valid = validate_growth_tactic_lessons_world(*this);
  if (!valid.ok)
    return valid;
  valid = validate_military_tactic_learning_world_sources(*this);
  if (!valid.ok)
    return valid;
  valid = validate_military_siege_engineering_world_actor_sources(*this);
  if (!valid.ok)
    return valid;
  if (growth_tactic_lessons_enabled() &&
      (!catalog->task_shop_order_contacts_enabled &&
       !catalog->task_message_returns && !catalog->task_personal_messages)) {
    auto inheritance = family_inheritance_access();
    valid = validate_family_inheritance(
        common, family_bequest, family_life, *catalog->interaction,
        inheritance.player, inheritance.received_notice,
        catalog->family_inheritance ? *catalog->family_inheritance
                                    : FamilyInheritancePolicy{},
        family_inheritance, inheritance.clearance);
    if (!valid.ok)
      return valid;
    if (player_plan_enabled()) {
      valid = validate_player_plan(common, player_plan, &growth_tactic_lessons);
      if (!valid.ok)
        return valid;
      if ((!catalog->task_shop_order_contacts_enabled &&
           !catalog->task_message_returns &&
           !catalog->task_personal_messages)) {
        if (!family_voluntary_paths_enabled()) {
          valid = validate_player_plan_world(*this);
          if (!valid.ok)
            return valid;
        }
      }
    }
  }
  if (catalog->task_shop_order_contacts_enabled ||
      catalog->task_message_returns || catalog->task_personal_messages) {
    auto inheritance = family_inheritance_access();
    valid = validate_family_inheritance(
        common, family_bequest, family_life, *catalog->interaction,
        inheritance.player, inheritance.received_notice,
        catalog->family_inheritance ? *catalog->family_inheritance
                                    : FamilyInheritancePolicy{},
        family_inheritance, inheritance.clearance);
    if (!valid.ok)
      return valid;
    if (player_plan_enabled()) {
      valid = validate_player_plan(
          common, player_plan,
          growth_tactic_lessons_enabled() ? &growth_tactic_lessons : nullptr,
          (catalog->task_personal_messages || catalog->task_personal_reports)
              ? &tasks
              : nullptr);
      if (!valid.ok)
        return valid;
      if (!family_voluntary_paths_enabled()) {
        valid = validate_player_plan_world(*this);
        if (!valid.ok)
          return valid;
      }
    }
  }
  if (family_voluntary_paths_enabled() && player_plan_enabled()) {
    valid = validate_player_plan(
        common, player_plan,
        growth_tactic_lessons_enabled() ? &growth_tactic_lessons : nullptr,
        (catalog->task_personal_messages || catalog->task_personal_reports)
            ? &tasks
            : nullptr);
    if (!valid.ok)
      return valid;
    valid = validate_player_plan_world(*this);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
// 各参与者/所有预留先释放，行动头和typed
// payload最后删除，永久事实仍由各域保留。
Status release_action(WorldBundle &w, const StableId &id) {
  auto p = w.common.action(id);
  if (!p)
    return common_failure("UnknownAction", id, ErrorKind::internal);
  const auto action = *p;
  auto lesson_release = guard_growth_tactic_lesson_release(w, id);
  if (!lesson_release.ok)
    return lesson_release;
  auto recovery = prepare_growth_recovery_release(w, id);
  if (!recovery.ok)
    return recovery;
  if (population_military_command_kind(action.kind) &&
      w.population_military_enabled()) {
    auto *history = w.population_military.action_facts.find(id);
    if (history && history->phase == "started") {
      auto context = w.population_write();
      auto stopped = cancel_population_military(context, id);
      if (!stopped.ok)
        return stopped;
    }
    w.population_military.actions.erase(id);
  }
  auto r = release_action_occupation_in_candidate(w.common, id);
  if (!r.ok)
    return r;
  r = erase_travel_action_payload(w.travel, id);
  if (!r.ok)
    return r;
  if (w.regional_life_enabled()) {
    r = cancel_life_activity(w.common, w.growth, id, w.common.tick(),
                             "ActionCancelled");
    if (!r.ok)
      return r;
  }
  w.tasks.actions.erase(id);
  w.family.actions.erase(id);
  w.growth.actions.erase(id);
  r = erase_estate_action_payload(w.estate, id);
  if (!r.ok)
    return r;
  // 完工owner可能已移除自己的运行载荷；统一释放仍按同一ID幂等移除，永久事实与训练条件不在这里删改。
  w.military.actions.erase(
      std::remove_if(w.military.actions.begin(), w.military.actions.end(),
                     [&](const auto &row) { return row.action_id == id; }),
      w.military.actions.end());
  w.military.logistics_actions.erase(
      std::remove_if(w.military.logistics_actions.begin(),
                     w.military.logistics_actions.end(),
                     [&](const auto &row) { return row.action_id == id; }),
      w.military.logistics_actions.end());
  w.military.training_actions.erase(
      std::remove_if(w.military.training_actions.begin(),
                     w.military.training_actions.end(),
                     [&](const auto &row) { return row.action_id == id; }),
      w.military.training_actions.end());
  return erase_released_action_in_candidate(w.common, id);
}
} // namespace hanmo::resident
