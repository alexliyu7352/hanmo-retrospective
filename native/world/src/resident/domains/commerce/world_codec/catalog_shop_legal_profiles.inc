// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
Integer selected_shop_licensing_source_version = 0;
if (const auto *dynamic = scenario.find("dynamic_world")) {
  const auto &caps = required_capabilities();
  if (result->growth_market_history &&
      std::find(caps.begin(), caps.end(), "growth_market_history_v1") ==
          caps.end())
    fail("GrowthMarketHistoryCapabilityRequired",
         "dynamic_world/growth_market_history");
  auto shops = [&]() {
    ScopedCheckpointJsonDecode original(
        CheckpointCatalogJsonKind::private_shops);
    return decode_private_shop_profile(scenario, caps);
  }();
  if (!shops.ok)
    return {false, {}, shops.error};
  const auto *law = dynamic->find("political_shop_licensing");
  if (const auto *registry =
          dynamic->find("political_shop_registry_successions")) {
    if (!law || !dynamic->find("private_shops") ||
        !result->political_takeover ||
        std::find(caps.begin(), caps.end(),
                  "political_shop_registry_successions_v1") == caps.end())
      fail("PoliticalShopRegistrySuccessionDependenciesRequired",
           "dynamic_world/political_shop_registry_successions");
    auto parsed = decode_political_shop_registry_catalog(*registry);
    if (!parsed.ok)
      return {false, {}, parsed.error};
    result->political_shop_registry_successions =
        checkpoint_make_shared<const PoliticalShopRegistrySuccessionCatalog>(
            std::move(parsed.value));
  }
  if (result->director && result->director->shop_registry_intents) {
    if (!result->political_shop_registry_successions || !result->common ||
        std::find(caps.begin(), caps.end(),
                  "political_shop_registry_intents_v1") == caps.end())
      fail("PoliticalShopRegistryIntentProfileRequired", "director");
    auto checked = validate_political_shop_registry_intents(
        *result->director->shop_registry_intents, *result->common,
        *result->political_shop_registry_successions);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  auto legal_receipts =
      decode_political_shop_licensing_receipt_profile(scenario, caps);
  if (!legal_receipts.ok)
    return {false, {}, legal_receipts.error};
  result->political_shop_licensing_receipts = legal_receipts.value;
  auto continued =
      decode_political_governance_continuation_profile(scenario, caps);
  if (!continued.ok)
    return {false, {}, continued.error};
  result->political_governance_continuations = continued.value;
  auto invitation_replies =
      decode_governance_invitation_reply_profile(scenario, caps);
  if (!invitation_replies.ok)
    return {false, {}, invitation_replies.error};
  result->political_governance_invitation_replies = invitation_replies.value;
  auto handovers = decode_political_head_handover_profile(scenario, caps);
  if (!handovers.ok)
    return {false, {}, handovers.error};
  if (handovers.value)
    result->political_head_handovers =
        checkpoint_make_shared<const PoliticalHeadHandoverCatalog>(
            std::move(*handovers.value));
  auto rounds = decode_political_successor_round_profile(scenario, caps);
  if (!rounds.ok)
    return {false, {}, rounds.error};
  if (rounds.value)
    result->political_successor_rounds =
        checkpoint_make_shared<const PoliticalSuccessorRoundCatalog>(
            std::move(*rounds.value));
  auto term_handovers = decode_political_term_handover_profile(scenario, caps);
  if (!term_handovers.ok)
    return {false, {}, term_handovers.error};
  if (term_handovers.value)
    result->political_term_handovers =
        checkpoint_make_shared<const PoliticalTermHandoverCatalog>(
            std::move(*term_handovers.value));
  auto office_receptions =
      decode_political_office_reception_profile(scenario, caps);
  if (!office_receptions.ok)
    return {false, {}, office_receptions.error};
  if (office_receptions.value)
    result->political_office_receptions =
        checkpoint_make_shared<const PoliticalOfficeReceptionCatalog>(
            std::move(*office_receptions.value));
  auto office_resignations =
      decode_political_office_resignation_profile(scenario, caps);
  if (!office_resignations.ok)
    return {false, {}, office_resignations.error};
  if (office_resignations.value)
    result->political_office_resignations =
        checkpoint_make_shared<const PoliticalOfficeResignationCatalog>(
            std::move(*office_resignations.value));
  if (result->director && result->director->office_resignation_responses) {
    if (!result->political_office_resignations ||
        !result->political_successions || !result->organization)
      return {
          false,
          {},
          common_failure("OfficeResignationResponseVersionPairRequired").error};
    auto linked = validate_political_office_resignation_responses(
        *result->director->office_resignation_responses, *result->common,
        *result->organization, *result->political_successions,
        *result->political_office_resignations);
    if (!linked.ok)
      return {false, {}, linked.error};
  }
  auto subordinate_appointments =
      decode_political_subordinate_appointment_profile(scenario, caps);
  if (!subordinate_appointments.ok)
    return {false, {}, subordinate_appointments.error};
  if (subordinate_appointments.value)
    result->political_subordinate_appointments =
        checkpoint_make_shared<const PoliticalSubordinateAppointmentCatalog>(
            std::move(*subordinate_appointments.value));
  auto subordinate_intents =
      decode_political_subordinate_intent_profile(scenario, caps);
  if (!subordinate_intents.ok)
    return {false, {}, subordinate_intents.error};
  if (subordinate_intents.value) {
    if (!result->director)
      return {
          false,
          {},
          common_failure("PoliticalSubordinateIntentProfileRequired").error};
    result->political_subordinate_appointment_intents = checkpoint_make_shared<
        const PoliticalSubordinateAppointmentIntentCatalog>(
        std::move(*subordinate_intents.value));
    auto director = checkpoint_make_shared<DirectorCatalog>(*result->director);
    director->subordinate_appointments = true;
    result->director = std::move(director);
  }
  auto dismissal_sources =
      decode_political_dismissal_source_profile(scenario, caps);
  if (!dismissal_sources.ok)
    return {false, {}, dismissal_sources.error};
  if (dismissal_sources.value)
    result->political_dismissal_sources =
        checkpoint_make_shared<const PoliticalDismissalSourceCatalog>(
            std::move(*dismissal_sources.value));
  auto governance_agencies =
      decode_political_governance_agency_profile(scenario, caps);
  if (!governance_agencies.ok)
    return {false, {}, governance_agencies.error};
  if (governance_agencies.value)
    result->political_governance_agencies =
        checkpoint_make_shared<const PoliticalGovernanceAgencyCatalog>(
            std::move(*governance_agencies.value));
  if (result->director && result->director->governance_agency_intents) {
    if (!result->political_governance_agencies || !result->politics ||
        !result->organization)
      return {
          false,
          {},
          common_failure("GovernanceAgencyIntentVersionPairRequired").error};
    auto linked = validate_political_governance_agency_intents(
        *result->director->governance_agency_intents, *result->common,
        *result->politics, *result->organization,
        *result->political_governance_agencies);
    if (!linked.ok)
      return {false, {}, linked.error};
  }
  auto successions = decode_political_succession_profile(scenario, caps);
  if (!successions.ok)
    return {false, {}, successions.error};
  if (successions.value)
    result->political_successions =
        checkpoint_make_shared<const PoliticalSuccessionCatalog>(
            std::move(*successions.value));
  auto daily_rent = decode_private_shop_daily_rent_profile(scenario, caps);
  if (!daily_rent.ok)
    return {false, {}, daily_rent.error};
  if (daily_rent.value)
    result->private_shop_daily_rent =
        checkpoint_make_shared<const PrivateShopDailyRentCatalog>(
            std::move(*daily_rent.value));
  auto shop_results =
      decode_private_shop_command_results_profile(scenario, caps);
  if (!shop_results.ok)
    return {false, {}, shop_results.error};
  if (shop_results.value)
    result->private_shop_command_results =
        checkpoint_make_shared<const PrivateShopCommandResultsCatalog>(
            std::move(*shop_results.value));
  if (bool(shops.value) != bool(law) ||
      (shops.value &&
       (!result->growth_market_history || !politics_enabled(*result))))
    fail("PrivateShopLicensingPairRequired", "dynamic_world");
  if (law) {
    if (std::find(caps.begin(), caps.end(), "political_shop_licensing_v1") ==
        caps.end())
      fail("PoliticalShopLicensingCapabilityRequired",
           "dynamic_world/political_shop_licensing");
    auto parsed = decode_political_shop_licensing_author_profile(*law);
    if (!parsed.ok)
      return {false, {}, parsed.error};
    selected_shop_licensing_source_version = parsed.value.source_version;
    if ((shops.value->version == 3 &&
         selected_shop_licensing_source_version != 2) ||
        (shops.value->version != 3 &&
         selected_shop_licensing_source_version != 1))
      fail("PrivateShopLicensingPairRequired", "dynamic_world");
    result->political_shop_licensing =
        checkpoint_make_shared<const PoliticalShopLicensingCatalog>(
            std::move(parsed.value.catalog));
    result->private_shops = checkpoint_make_shared<const PrivateShopCatalog>(
        std::move(*shops.value));
  }
  if (result->task_shop_orders_enabled !=
      bool(result->private_shops &&
           !result->private_shops->orders.policies.empty()))
    fail("TaskShopOrderProfileMismatch", "dynamic_world/private_shops/orders");
  auto liquidations = decode_private_shop_liquidation_profile(scenario, caps);
  if (!liquidations.ok)
    return {false, {}, liquidations.error};
  if (liquidations.value) {
    if (!result->task_message_notices ||
        !result->task_message_handoffs_enabled || !result->tasks ||
        !result->private_shops || !result->task_shop_orders_enabled)
      fail("PrivateShopLiquidationDependencyRequired",
           "private_shop_liquidations");
    auto routes = validate_private_shop_liquidation_notice_routes(
        *liquidations.value, *result->tasks, *result->task_message_notices);
    if (!routes.ok)
      return {false, {}, routes.error};
    result->private_shop_liquidations =
        checkpoint_make_shared<const PrivateShopLiquidationCatalog>(
            std::move(*liquidations.value));
  }
}
