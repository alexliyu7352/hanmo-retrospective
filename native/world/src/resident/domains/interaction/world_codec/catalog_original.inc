// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto i = decode_interaction_catalog(data, result->common);
if (!i.ok)
  return {false, {}, i.error};
if (!result->task_offer_responses) {
  std::function<bool(const Condition &)> uses_revisit =
      [&](const Condition &q) {
        if (q.kind == Condition::Kind::offer_declined_before)
          return true;
        for (const auto &term : q.terms)
          if (uses_revisit(term))
            return true;
        return false;
      };
  const auto *enabled = scenario.find("enabled");
  const auto *selected = enabled ? enabled->find("dialogues") : nullptr;
  const auto selected_dialogues =
      selected ? strings(*selected, "scenario/enabled/dialogues")
               : std::vector<StableId>{};
  for (const auto &dialogue : i.value.dialogues) {
    if (selected &&
        std::find(selected_dialogues.begin(), selected_dialogues.end(),
                  dialogue.id) == selected_dialogues.end())
      continue;
    for (const auto &node : dialogue.nodes) {
      if (node.condition && uses_revisit(*node.condition))
        fail("TaskOfferResponseProfileRequired", "dialogues/" + dialogue.id);
      for (const auto &option : node.options)
        for (const auto &q : option.conditions)
          if (uses_revisit(q))
            fail("TaskOfferResponseProfileRequired",
                 "dialogues/" + dialogue.id);
    }
  }
}
if (result->regional_life) {
  auto configured = configure_regional_life(
      i.value, field(field(scenario, "dynamic_world", "scenario"),
                     "regional_life", "dynamic_world"));
  if (!configured.ok)
    return {false, {}, configured.error};
}
if (result->party_invitations_enabled) {
  const auto *block = scenario.find("party_invitations");
  if (!block)
    fail("MissingPartyInvitationCatalog", "party_invitations");
  auto decoded = decode_party_invitation_catalog(*block, i.value);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.party_invitations =
      checkpoint_make_shared<const PartyInvitationCatalog>(
          std::move(decoded.value));
}
if (recruitment.value) {
  auto decoded = decode_party_recruitment_consent_catalog(
      field(scenario, "party_recruitment_consents", "scenario"), i.value);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.party_recruitment_consents =
      checkpoint_make_shared<const PartyRecruitmentConsentCatalog>(
          std::move(decoded.value));
}
if (visit_messages.value) {
  auto decoded = decode_visit_message_catalog(
      field(scenario, "visit_messages", "scenario"), i.value);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.visit_messages = checkpoint_make_shared<const VisitMessageCatalog>(
      std::move(decoded.value));
}
auto response_selection =
    decode_native_interaction_response_selection(scenario);
if (!response_selection.ok)
  return {false, {}, std::move(response_selection.error)};
i.value.response_sources_version = response_selection.value;
result->interaction =
    checkpoint_make_shared<const InteractionCatalog>(std::move(i.value));
