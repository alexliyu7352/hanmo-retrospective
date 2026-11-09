// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
namespace {
// 原导出额外来源门的唯一实现：有树导出与无树组合均借同Common头，保持原遍历/首错次序。
// payload本身由调用者的同writer完整遍历；此函数不把存在一个键当作来源合法。
template <class HasPayload, class PutDomain, class PutPayload,
          class CheckDailyRent>
Status bind_world_export_facts(const WorldBundle &w, HasPayload has_payload,
                               PutDomain put_domain, PutPayload put_payload,
                               CheckDailyRent check_daily_rent) {
  for (const auto &row : w.common.state().facts.rows()) {
    const auto &h = row.second;
    const bool issue = h.kind == political_field_issue_kind,
               answer = h.kind == political_field_accept_kind ||
                        h.kind == political_field_decline_kind,
               withdraw = h.kind == political_field_withdraw_kind;
    if ((issue || answer || withdraw) &&
        (!w.political_field_appointments_enabled() || h.domain != "politics" ||
         !(issue    ? w.political_field_appointments.authorizations.find(
                          row.first) != nullptr
           : answer ? w.political_field_appointments.answers.find(row.first) !=
                          nullptr
                    : w.political_field_appointments.withdrawals.find(
                          row.first) != nullptr)))
      fail("MissingPoliticalFieldPayload", row.first);
  }
  auto work_headers = validate_task_work_original_fact_headers(
      w.common, w.tasks.work_execution ? &*w.tasks.work_execution : nullptr);
  if (!work_headers.ok)
    return work_headers;
  std::size_t legal_exported = 0, registry_exported = 0;
  bool relations_validated = false;
  for (const auto &row : w.common.state().facts.rows()) {
    const bool payload = has_payload(row.first);
    const auto *legal_header = w.common.state().facts.find(row.first);
    if (legal_header &&
        (legal_header->kind == "political_relations_initialized" ||
         legal_header->kind == "political_battle_relation_changed" ||
         legal_header->kind == "political_alliance_breached")) {
      // P3 正文只在原 political_relations 段；绑定之前先完整验证真实根，
      // 再按三个确切拥有位置认领头，不沿旧 politics 域的宽泛回退。
      if (payload || legal_header->domain != "politics" ||
          legal_header->id != row.first || !w.political_relations)
        fail("PoliticalRelationsOriginalSourceRequired", row.first);
      if (!relations_validated) {
        auto checked = validate_native_political_relations_world(w);
        if (!checked.ok)
          return checked;
        relations_validated = true;
      }
      const auto &state = *w.political_relations;
      bool owned = false;
      if (legal_header->kind == "political_relations_initialized")
        owned = state.initialization.domain == legal_header->domain &&
                state.initialization.kind == legal_header->kind &&
                state.initialization.id == row.first;
      else if (legal_header->kind == "political_battle_relation_changed") {
        const auto *original = state.battle_changes.find(row.first);
        owned = original && original->id == row.first;
      } else {
        const auto *original = state.alliance_breaches.find(row.first);
        owned = original && original->id == row.first;
      }
      if (!owned)
        fail("PoliticalRelationsOriginalSourceRequired", row.first);
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->kind == "actor_social_occupancy_changed") {
      // 正文唯一存于 actor_social_occupancy 原段；导出只绑定真实 transition。
      if (payload || legal_header->domain != "interaction" ||
          legal_header->id != row.first || !w.interaction.social_occupancy ||
          !w.interaction.social_occupancy->transitions.find(row.first))
        fail("SocialOccupancyTransitionMissing", row.first);
      auto checked = validate_native_actor_social_occupancy(w);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->kind == "political_passage_decided") {
      if (payload || legal_header->domain != "politics" || !w.political_passage)
        fail("PoliticalPassageOriginalPayloadRequired", row.first);
      auto original = read_political_passage_decision(w.common, *w.political_passage,
          {"politics", "political_passage_decided", row.first});
      if (!original.ok) return {false, {}, std::move(original.error)};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->kind == "growth_result_terminal") {
      // 正文在独立原 typed 段；只绑定这个已验终态头，不豁免整个 growth 域。
      const auto *publication = w.growth.terminal_publications.find(row.first);
      if (payload || !w.catalog || !w.catalog->exact_time ||
          !exact_complete_selected(*w.catalog->exact_time) ||
          !w.catalog->growth || legal_header->domain != "growth" ||
          legal_header->id != row.first || !publication ||
          publication->publication_id != row.first)
        fail("GrowthTerminalPublicationPayloadMismatch", row.first);
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && w.military_action_time) {
      auto selected = selected_native_military_state_versions(w.catalog->identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      auto owned = military_action_time_owns_fact_header(*w.military_action_time,
                                                        *legal_header);
      if (!owned.ok)
        return {false, {}, std::move(owned.error)};
      if (owned.value) {
        if (payload || selected.value.military_action_time != 1)
          fail("MilitaryOriginalSourceCoverageMismatch", row.first);
        put_domain(row.first, legal_header->domain);
        continue;
      }
    }
    if (legal_header &&
        legal_header->kind == "military_training_completion_applied") {
      if (payload || !w.military_action_time)
        fail("MilitaryTrainingCompletionHeaderMismatch", row.first);
      auto selected =
          selected_native_military_state_versions(w.catalog->identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      auto checked = validate_military_training_completion_header_source(
          *legal_header, w.military, *w.military_action_time,
          selected.value.military_training_completion_time);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (legal_header->kind == "campaign_accepted" ||
         legal_header->kind == "military_office_completion_applied" ||
         legal_header->kind == "military_formation_completion_checked")) {
      if (payload || !w.military_action_time)
        fail("MilitaryAuthorityHeaderMismatch", row.first);
      auto selected =
          selected_native_military_state_versions(w.catalog->identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      auto checked = validate_military_authority_header_source(
          *legal_header, w.military, *w.military_action_time,
          selected.value.military_authority_time,
          selected.value.military_action_time);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (legal_header->kind == political_shop_registry_accept_kind ||
         legal_header->domain == political_shop_registry_succession_module)) {
      if (!w.political_shop_registry_successions_enabled() || payload)
        fail("PoliticalShopRegistryPayloadRequired", row.first);
      auto checked = validate_political_shop_registry_fact_header(
          w.common, w.political_shop_registry_successions, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      ++registry_exported;
      continue;
    }
    if (legal_header && legal_header->domain == "travel" &&
        legal_header->kind == "movement_boundary_contact") {
      if (payload)
        fail("DuplicateMovementBoundaryFactPayload", row.first);
      auto checked = validate_native_movement_boundary_layout(w);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->domain == "travel" &&
        legal_header->kind == "movement_boundary_continuation") {
      if (payload)
        fail("DuplicateMovementBoundaryContinuationFactPayload", row.first);
      auto checked = validate_native_movement_boundary_continuation_layout(w);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->domain == "travel" &&
        legal_header->kind == "local_move") {
      if (payload)
        fail("DuplicateLocalMoveFactPayload", row.first);
      auto checked = validate_native_local_move_layout(w);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->domain == "travel" &&
        (legal_header->kind == "group_contact_member_release" ||
         legal_header->kind == "group_member_loss_handoff")) {
      if (payload)
        fail("DuplicateGroupMemberHandoffFactPayload", row.first);
      auto checked = validate_native_group_member_handoff_layout(w);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && owns_transport_physical_fact_kind(legal_header->domain,
                                                          legal_header->kind)) {
      if (payload)
        fail("DuplicateTransportPhysicalFactPayload", row.first);
      auto roots =
          native_transport_physical_roots(const_cast<WorldBundle &>(w));
      if (!roots.ok)
        return {false, {}, std::move(roots.error)};
      if (!roots.value)
        fail("TransportPhysicalFactStateRequired", row.first);
      auto checked = validate_transport_physical_fact_header(
          w.common, *roots.value, *legal_header);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        owns_task_exact_original_fact_kind(legal_header->kind)) {
      if (!payload)
        fail("TaskExecutionOriginalFactPayloadRequired", row.first);
      auto selected =
          selected_native_task_execution_version(w.catalog->identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      auto schema = read_task_exact_original_fact_schema(
          w.common, w.tasks, *legal_header, selected.value);
      if (!schema.ok)
        return {false, {}, std::move(schema.error)};
      if (!schema.value)
        fail("TaskExecutionOriginalFactPayloadRequired", row.first);
      put_payload(row.first);
      continue;
    }
    if (legal_header &&
        is_native_economic_facility_fact_kind(legal_header->kind)) {
      if (!payload)
        fail("EconomicFacilityOriginalFactPayloadRequired", row.first);
      auto schema = read_native_economic_facility_fact_schema(
          w.common, *w.catalog,
          w.economic_facility_capacity_sources
              ? &*w.economic_facility_capacity_sources
              : nullptr,
          w.production_facility_conditions ? &*w.production_facility_conditions
                                           : nullptr,
          w.governance_work_expense ? &*w.governance_work_expense : nullptr,
          *legal_header);
      if (!schema.ok)
        return {false, {}, std::move(schema.error)};
      if (schema.value != 1)
        fail("EconomicFacilityOriginalFactPayloadRequired", row.first);
      put_payload(row.first);
      continue;
    }
    if (legal_header && legal_header->domain == "military" &&
        (legal_header->kind == "military_battle_started" ||
         legal_header->kind == "military_battle_closed")) {
      if (payload)
        fail("DuplicateMilitaryBattleLifecycleFactPayload", row.first);
      auto checked = validate_native_military_battle_lifecycle_sources(w);
      if (!checked.ok)
        return checked;
      auto selected =
          selected_native_military_battle_lifecycle(w.catalog->identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      if (selected.value != 1)
        fail("MilitaryBattleLifecycleSourceRequired", row.first);
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->domain == "personal" &&
        (legal_header->kind == "personal_battle_started" ||
         legal_header->kind == "personal_battle_closed")) {
      if (payload)
        fail("DuplicatePersonalBattleLifecycleFactPayload", row.first);
      auto checked = validate_native_personal_battle_lifecycle_sources(w);
      if (!checked.ok)
        return checked;
      auto selected =
          selected_native_personal_battle_lifecycle(w.catalog->identity);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      if (selected.value != 1)
        fail("PersonalBattleLifecycleSourceRequired", row.first);
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && owns_task_work_original_fact_kind(legal_header->kind)) {
      if (payload)
        fail("DuplicateTaskWorkFactPayload", row.first);
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && is_native_production_cycle_fact(*legal_header)) {
      if (!w.production_cycles || payload)
        fail("ProductionCyclesFactPayloadMismatch", row.first);
      auto checked = validate_native_production_cycle_fact(*w.production_cycles,
                                                           *legal_header);
      if (!checked.ok)
        return checked;
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header && legal_header->domain == "commerce" &&
        legal_header->kind == "self_transport_funding_accepted") {
      if (!w.catalog->private_transport_funding_selected ||
          !w.private_transport_funding || payload)
        fail("PrivateTransportFundingFactMismatch", row.first);
      const auto *original = w.private_transport_funding->terms.find(row.first);
      if (!original || original->id != row.first ||
          original->acceptance_ref.domain != legal_header->domain ||
          original->acceptance_ref.kind != legal_header->kind ||
          original->acceptance_ref.id != row.first)
        fail("PrivateTransportFundingFactMismatch", row.first);
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (legal_header->kind == political_governance_continue_kind ||
         legal_header->domain == political_governance_continuation_module)) {
      if (!w.catalog->political_governance_continuations ||
          !w.political_governance_continuations || payload)
        fail("GovernanceContinuationFactMismatch", row.first);
      auto checked = validate_political_governance_continuation_fact_header(
          w.common, *w.political_governance_continuations, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (legal_header->kind == governance_invitation_reply_fact_kind ||
         legal_header->domain == governance_invitation_reply_module)) {
      if (!w.catalog->political_governance_invitation_replies ||
          !w.governance_invitation_replies || payload)
        fail("GovernanceInvitationFactMismatch", row.first);
      auto checked = validate_governance_invitation_reply_fact_header(
          w.common, *w.governance_invitation_replies, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_head_handover_fact_kind(legal_header->kind) ||
         legal_header->domain == political_head_handover_domain)) {
      if (!w.catalog->political_head_handovers || !w.political_head_handovers ||
          payload)
        fail("PoliticalHeadHandoverFactMismatch", row.first);
      auto checked = validate_political_head_handover_fact_header(
          w.common, *w.political_head_handovers, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_successor_round_fact_kind(legal_header->kind) ||
         legal_header->domain == political_successor_rounds_domain)) {
      if (!w.catalog->political_successor_rounds ||
          !w.political_successor_rounds || payload)
        fail("PoliticalSuccessorRoundFactMismatch", row.first);
      auto checked = validate_political_successor_round_fact_header(
          w.common, *w.political_successor_rounds, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_term_handover_fact_kind(legal_header->kind) ||
         legal_header->domain == political_term_handover_domain)) {
      if (!w.catalog->political_term_handovers || !w.political_term_handovers ||
          payload)
        fail("PoliticalTermHandoverFactMismatch", row.first);
      auto checked = validate_political_term_handover_fact_header(
          w.common, *w.political_term_handovers, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_office_reception_fact_kind(legal_header->kind) ||
         legal_header->domain == political_office_reception_domain)) {
      if (!w.catalog->political_office_receptions ||
          !w.political_office_receptions || payload)
        fail("PoliticalOfficeReceptionFactMismatch", row.first);
      auto checked = validate_political_office_reception_fact_header(
          w.common, *w.political_office_receptions, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_office_resignation_fact_kind(legal_header->kind) ||
         legal_header->domain == political_office_resignation_module)) {
      if (!w.catalog->political_office_resignations ||
          !w.political_office_resignations || payload)
        fail("PoliticalOfficeResignationFactMismatch", row.first);
      auto checked = validate_political_office_resignation_fact_header(
          w.common, *w.political_office_resignations, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_subordinate_appointment_fact_kind(legal_header->kind) ||
         legal_header->domain == political_subordinate_appointment_module)) {
      if (!w.catalog->political_subordinate_appointments ||
          !w.political_subordinate_appointments || payload)
        fail("PoliticalSubordinateAppointmentFactMismatch", row.first);
      auto checked = validate_political_subordinate_appointment_fact_header(
          w.common, *w.political_subordinate_appointments, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_governance_agency_fact_kind(legal_header->kind) ||
         legal_header->domain == political_governance_agency_domain)) {
      if (!w.catalog->political_governance_agencies ||
          !w.political_governance_agencies || payload)
        fail("PoliticalGovernanceAgencyFactMismatch", row.first);
      auto checked = validate_political_governance_agency_fact_header(
          w.common, *w.political_governance_agencies, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (is_political_succession_fact_kind(legal_header->kind) ||
         legal_header->domain == political_succession_domain)) {
      if (!w.catalog->political_successions || !w.political_successions ||
          payload)
        fail("PoliticalSuccessionFactMismatch", row.first);
      auto checked = validate_political_succession_fact_header(
          w.common, *w.political_successions, *legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header &&
        (daily_rent_fact_kind(legal_header->kind) ||
         legal_header->domain == private_shop_daily_rent_module)) {
      if (!w.private_shop_daily_rent_enabled() || payload)
        fail("PrivateShopDailyRentFactMismatch", row.first);
      auto checked = check_daily_rent(*legal_header);
      if (!checked.ok)
        return {false, {}, checked.error};
      put_domain(row.first, legal_header->domain);
      continue;
    }
    if (legal_header) {
      const auto &kind = legal_header->kind;
      if (legal_header->domain == political_shop_licensing_module ||
          kind == "political_shop_registry_initialized" ||
          kind == "political_wanted_registered" ||
          kind == "political_wanted_released" ||
          kind == "political_shop_clearance_recorded" ||
          kind == "political_shop_permit_issued" ||
          kind == "political_shop_permit_revoked") {
        if (!w.political_shop_licensing_enabled())
          fail("PoliticalShopLicensingProfileRequired", row.first);
        auto checked = validate_political_shop_licensing_fact_header(
            w.common, w.political_shop_licensing, *legal_header);
        if (!checked.ok)
          return {false, {}, checked.error};
        if (payload)
          fail("PoliticalShopPayloadMismatch", row.first);
        put_domain(row.first, legal_header->domain);
        ++legal_exported;
        continue;
      }
    }
    if (!payload) {
      auto *header = w.common.state().facts.find(row.first);
      if (w.dynamic_enabled() && header &&
          ((w.naval_enabled() && header->domain == "military_naval" &&
            (header->kind == "naval_form" || header->kind == "naval_disband" ||
             header->kind == "naval_contact" ||
             header->kind == "naval_battle" || header->kind == "naval_plan" ||
             header->kind == "naval_round" || header->kind == "naval_capture" ||
             header->kind == "naval_decline" ||
             header->kind == "naval_continue" ||
             (w.naval_navigation_enabled() &&
              ((header->kind == "naval_voyage" &&
                std::any_of(
                    w.naval.navigation.voyages.begin(),
                    w.naval.navigation.voyages.end(),
                    [&](const auto &f) { return f.id == row.first; })) ||
               (header->kind == "naval_navigation_step" &&
                std::any_of(
                    w.naval.navigation.steps.begin(),
                    w.naval.navigation.steps.end(),
                    [&](const auto &f) { return f.id == row.first; })))) ||
             (w.naval_repairs_enabled() &&
              ((header->kind == "naval_repair_start" &&
                std::any_of(
                    w.naval.repairs.starts.begin(),
                    w.naval.repairs.starts.end(),
                    [&](const auto &f) { return f.id == row.first; })) ||
               (header->kind == "naval_repair_result" &&
                std::any_of(
                    w.naval.repairs.results.begin(),
                    w.naval.repairs.results.end(),
                    [&](const auto &f) { return f.id == row.first; })))))) ||
           (w.economic_supply_policies_enabled() &&
            header->domain == economic_supply_policy_module &&
            ((header->kind == economic_supply_policy_issue_kind &&
              w.economic_supply_policies.policies.find(row.first)) ||
             (header->kind == economic_supply_observation_kind &&
              w.economic_supply_policies.observations.find(row.first)))) ||
           (w.estate_employment_enabled() &&
            header->domain == "estate_employment" &&
            ((header->kind == "estate_employment_offered" &&
              w.estate_employment.contracts.find(row.first)) ||
             (header->kind == "estate_employment_decided" &&
              w.estate_employment.decisions.find(row.first)) ||
             (header->kind == "estate_employment_service" &&
              w.estate_employment.services.find(row.first)))) ||
           (w.family_birth_enabled() && header->domain == "family_birth" &&
            validate_birth_fact_header(w.family_birth, *header).ok) ||
           (w.family_marriage_enabled() &&
            header->domain == "family_marriage" &&
            ((header->kind == "family_marriage_proposed" &&
              w.family_marriage.proposals.find(row.first)) ||
             (header->kind == "family_marriage_replied" &&
              w.family_marriage.replies.find(row.first)) ||
             (header->kind == "family_marriage_dissolved" &&
              w.family_marriage.dissolutions.find(row.first)))) ||
           (w.family_inheritance_enabled() &&
            header->domain == "family_inheritance" &&
            ((header->kind == "family_bequest_disclosed" &&
              w.family_inheritance.disclosures.find(row.first)) ||
             (header->kind == "family_bequest_accepted" &&
              w.family_inheritance.acceptances.find(row.first)) ||
             (header->kind == "family_inheritance_settled" &&
              w.family_inheritance.settlements.find(row.first)))) ||
           (w.family_bequest_enabled() && header->domain == "family_bequest" &&
            ((header->kind == "family_bequest_declaration" &&
              w.family_bequest.declarations.find(row.first)) ||
             (header->kind == "family_bequest_revocation" &&
              w.family_bequest.revocations.find(row.first)))) ||
           (w.family_legacy_enabled() && header->domain == "family_legacy" &&
            ((header->kind == "family_legacy_control_consent" &&
              w.family_legacy.consents.find(row.first)) ||
             (header->kind == "family_legacy_control_revocation" &&
              w.family_legacy.revocations.find(row.first)) ||
             (header->kind == "family_legacy_control_activated" &&
              w.family_legacy.activations.find(row.first)))) ||
           (w.family_life_enabled() && header->domain == "family_life" &&
            header->kind == "family_death" &&
            w.family_life.deaths.find(row.first)) ||
           (w.life_age_checks && header->domain == "family_life" &&
            ((header->kind == "age_check" &&
              w.life_age_checks->checks.find(row.first)) ||
             (header->kind == "life_age_settings_changed" &&
              w.life_age_checks->settings_changes.find(row.first)))) ||
           ((header->domain == "politics" &&
             ((header->kind != treaty_passage_delivery_kind &&
               header->kind != treaty_passage_signature_kind) ||
              (w.treaty_passage_enabled() &&
               ((header->kind == treaty_passage_delivery_kind &&
                 w.treaty_passage.deliveries.find(row.first)) ||
                (header->kind == treaty_passage_signature_kind &&
                 w.treaty_passage.signatures.find(row.first)))))) &&
            (header->domain == "politics" &&
             ((header->kind != political_officer_issue_kind &&
               header->kind != political_officer_accept_kind &&
               header->kind != political_officer_decline_kind &&
               header->kind != political_officer_withdraw_kind) ||
              (w.political_military_appointments_enabled() &&
               ((header->kind == political_officer_issue_kind &&
                 w.political_military_appointments.authorizations.find(
                     row.first)) ||
                ((header->kind == political_officer_accept_kind ||
                  header->kind == political_officer_decline_kind) &&
                 w.political_military_appointments.answers.find(row.first)) ||
                (header->kind == political_officer_withdraw_kind &&
                 w.political_military_appointments.withdrawals.find(
                     row.first))))))) ||
           (w.family_care_relief_enabled() &&
            header->domain == "family_care_relief" &&
            validate_child_care_relief_fact_header(w.child_care_relief, *header)
                .ok) ||
           (w.personal_recovery_enabled() &&
            header->domain == "personal_injury_recovery" &&
            ((header->kind == "personal_recovery_credit" &&
              w.personal_recovery.credits.find(row.first)) ||
             (header->kind == "personal_recovery_closed" &&
              w.personal_recovery.closures.find(row.first)))) ||
           (w.catalog->director && w.catalog->director->visit_reception &&
            w.interaction.visit_reception_claims &&
            header->domain == visit_reception_claim_module &&
            header->kind == visit_reception_claim_kind &&
            w.interaction.visit_reception_claims->claims.find(row.first)) ||
           (w.catalog->interaction->visit_messages &&
            w.interaction.visit_messages &&
            header->domain == "visit_messages" &&
            ((header->kind == visit_message_posted_kind &&
              w.interaction.visit_messages->posted.find(row.first)) ||
             (header->kind == visit_message_received_kind &&
              w.interaction.visit_messages->received.find(row.first)) ||
             (header->kind == visit_message_withdrawn_kind &&
              w.interaction.visit_messages->withdrawn.find(row.first)))) ||
           header->domain == "local_economy" ||
           header->domain == "organization" ||
           header->domain == "population_military" ||
           (w.siege_integration_enabled() &&
            (header->kind == "siege_integration_proposal" ||
             header->kind == "siege_integration_actor_action" ||
             header->kind == "siege_officer_decision" ||
             header->kind == "military_integration")) ||
           header->kind == "task_delivery" || header->kind == "task_transfer" ||
           (w.catalog->task_extensions && w.tasks.deadline_extensions &&
            header->domain == task_extension_module &&
            header->kind == task_extension_fact_kind &&
            w.tasks.deadline_extensions->facts.find(row.first)) ||
           (w.catalog->task_static_assets_enabled && w.tasks.static_assets &&
            header->domain == "tasks" &&
            header->kind == task_static_asset_fact_kind &&
            w.tasks.static_assets->receipts.find(row.first)) ||
           (w.catalog->task_message_notices && w.tasks.message_notices &&
            header->domain == "tasks" &&
            header->kind == task_death_notice_bound_kind &&
            w.tasks.message_notices->bindings.find(row.first)) ||
           (w.catalog->task_message_handoffs_enabled &&
            w.tasks.message_handoffs && header->domain == "tasks" &&
            ((header->kind == task_message_handoff_kind &&
              w.tasks.message_handoffs->receipts.find(row.first)) ||
             ((w.catalog->task_message_handoff_version == 2 ||
               w.catalog->task_message_handoff_version == 3 ||
               w.catalog->task_message_handoff_version == 4) &&
              w.tasks.message_handoffs->version ==
                  w.catalog->task_message_handoff_version &&
              header->kind == task_message_report_kind &&
              w.tasks.message_handoffs->reports.find(row.first)))) ||
           (w.catalog->task_personal_advances && w.tasks.personal_advances &&
            header->domain == "tasks" &&
            ((header->kind == task_advance_fact_kind &&
              w.tasks.personal_advances->payments.find(row.first)) ||
             (header->kind == task_advance_repayment_fact_kind &&
              w.tasks.personal_advances->repayments.find(row.first)))) ||
           (w.political_takeover_enabled() && header->domain == "tasks" &&
            w.tasks.takeover_execution &&
            ((header->kind == "task_contract_payment" &&
              w.tasks.takeover_execution->payments.find(row.first)) ||
             (header->kind == "task_static_payment" &&
              w.tasks.takeover_execution->version == 2 &&
              w.tasks.takeover_execution->static_payments.find(row.first)))))) {
        put_domain(row.first, header->domain);
        continue;
      }
      fail("MissingDomainFactPayload", row.first);
    }
    put_payload(row.first);
  }
  if (registry_exported !=
      w.political_shop_registry_successions.acceptances.size())
    fail("PoliticalShopRegistryPayloadRequired",
         "political_shop_registry_successions");
  {
    const auto &state = w.political_shop_licensing;
    if (legal_exported != state.registries.size() + state.warrants.size() +
                              state.releases.size() + state.clearances.size() +
                              state.permits.size() + state.revocations.size())
      fail("PoliticalShopPayloadMismatch", "political_shop_licensing");
  }
  return {true, {}, {}};
}
} // namespace
