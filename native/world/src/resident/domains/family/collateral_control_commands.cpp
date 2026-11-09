// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 三个新零时命令只写原认可或唯一Legacy账；死后激活继续使用原family_legacy_activate。
#include "collateral_control_commands.hpp"
#include "succession_personal_assets.hpp"
namespace hanmo::resident {
namespace {
bool reference(const StableId &s) { return !s.empty() && s.size() <= 256; }
bool closed(const FamilyLegacyState &state, const StableId &id) {
  for (const auto &r : state.revocations.rows())
    if (r.second.consent_fact_id == id)
      return true;
  for (const auto &a : state.activations.rows())
    if (a.second.consent_fact_id == id)
      return true;
  return false;
}
} // namespace
Outcome<CollateralControlCommand>
decode_collateral_control_command(const StableId &kind,
                                  const BoundaryValue &args) {
  try {
    auto valid = validate_succession_json_unique(args);
    if (!valid.ok)
      return {false, {}, valid.error};
    using namespace boundary;
    CollateralControlCommand cmd;
    cmd.kind = kind;
    if (kind == "family_collateral_recognize") {
      fields(args, {"request_id", "candidate_actor_id"}, {}, kind);
      cmd.candidate_actor_id = get_string(args, "candidate_actor_id", kind);
    } else if (kind == "family_collateral_accept") {
      fields(args, {"request_id", "candidate_actor_id", "recognition_fact_id"},
             {}, kind);
      cmd.candidate_actor_id = get_string(args, "candidate_actor_id", kind);
      cmd.recognition_fact_id = get_string(args, "recognition_fact_id", kind);
      if (!reference(cmd.recognition_fact_id))
        fail("InvalidCollateralControlCommand", kind);
    } else if (kind == "family_collateral_revoke") {
      fields(args, {"request_id", "consent_fact_id"}, {}, kind);
      cmd.consent_fact_id = get_string(args, "consent_fact_id", kind);
    } else
      fail("UnknownCollateralControlCommand", kind);
    cmd.request_id = get_string(args, "request_id", kind);
    if (!reference(cmd.request_id) ||
        !reference(kind == "family_collateral_revoke" ? cmd.consent_fact_id
                                                      : cmd.candidate_actor_id))
      fail("InvalidCollateralControlCommand", kind);
    return {true, std::move(cmd), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<CollateralControlQuote> preview_collateral_control_command(
    const CollateralControlDecisionContext &x, const FamilyState &family,
    const FamilyDeathState &deaths, const FamilyDeathOriginRead &death_read,
    const InteractionState &interactions, const InteractionAccess &access,
    const FamilyLegacyState &legacy, const FamilyLegacyPlayableRead &playable,
    const StableId &actor, const CollateralControlCommand &cmd,
    const DiscipleControlRelationContext *disciple,
    const BiologicalControlDecisionContext *biological) {
  const auto &r = x.recognition;
  const auto &c = r.people.common;
  if (actor != c.state().metadata.player_actor_id)
    return {false, {}, common_failure("PlayerCommandAuthorityRequired").error};
  auto valid = validate_collateral_control_context(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_collateral_recognition_history(r, x.state);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_family_legacy(
      c, family, deaths, death_read, r.people.catalog, r.interactions,
      r.original_requests, legacy, playable, disciple, biological, &x);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_collateral_control_result_bindings(c, legacy, r.catalog,
                                                      x.state);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!r.catalog.definitions.find(cmd.request_id))
    return {
        false, {}, common_failure("CollateralControlRequestMismatch").error};
  CollateralControlQuote out;
  if (cmd.kind == "family_collateral_recognize") {
    if (!reference(cmd.candidate_actor_id) ||
        !cmd.recognition_fact_id.empty() || !cmd.consent_fact_id.empty())
      return {
          false, {}, common_failure("InvalidCollateralControlCommand").error};
    for (const auto &row : x.state.recognitions.rows()) {
      const auto &s =
          std::get<CollateralRecognitionFact>(row.second.payload).source;
      if ((s.first_actor_id == actor &&
           s.second_actor_id == cmd.candidate_actor_id) ||
          (s.second_actor_id == actor &&
           s.first_actor_id == cmd.candidate_actor_id))
        return {false,
                {},
                common_failure("CollateralAlreadyRecognized", row.first).error};
    }
    auto quote = preview_collateral_recognition(
        r, actor, cmd.candidate_actor_id, cmd.request_id);
    if (!quote.ok)
      return {false, {}, quote.error};
    out.recognition = std::move(quote.value);
    return {true, std::move(out), {}};
  }
  StableId candidate = cmd.candidate_actor_id,
           recognition = cmd.recognition_fact_id;
  if (cmd.kind == "family_collateral_accept") {
    if (!reference(candidate) || !reference(recognition) ||
        !cmd.consent_fact_id.empty())
      return {
          false, {}, common_failure("InvalidCollateralControlCommand").error};
  } else if (cmd.kind == "family_collateral_revoke") {
    if (!candidate.empty() || !recognition.empty() ||
        !reference(cmd.consent_fact_id))
      return {
          false, {}, common_failure("InvalidCollateralControlCommand").error};
    const auto *old = legacy.consents.find(cmd.consent_fact_id);
    if (!old || !old->source.collateral_dialogue ||
        old->source.predecessor_actor_id != actor ||
        old->source.request_id != cmd.request_id)
      return {
          false,
          {},
          common_failure("CollateralControlRevocationSourceMismatch").error};
    if (closed(legacy, cmd.consent_fact_id))
      return {false,
              {},
              common_failure("LegacyConsentAlreadyClosed", cmd.consent_fact_id)
                  .error};
    candidate = old->source.successor_actor_id;
    recognition = old->source.relation_fact_id;
  } else
    return {false, {}, common_failure("UnknownCollateralControlCommand").error};
  auto quote = preview_collateral_control_decision(
      x, actor, candidate, cmd.request_id,
      cmd.kind == "family_collateral_accept" ? "accept" : "revoke",
      recognition);
  if (!quote.ok)
    return {false, {}, quote.error};
  if (cmd.kind == "family_collateral_accept")
    for (const auto &row : legacy.consents.rows()) {
      const auto &s = row.second.source;
      if (s.request_id == quote.value.decision.request_id &&
          s.session_id == quote.value.decision.session_id &&
          s.predecessor_actor_id == actor && s.successor_actor_id == candidate)
        return {false,
                {},
                common_failure("LegacyConsentSourceConsumed", row.first).error};
    }
  out.decision = std::move(quote.value);
  return {true, std::move(out), {}};
}
Outcome<FamilyEffect> apply_collateral_control_command(
    CommonStore &c, const CollateralControlDecisionContext &x,
    CollateralControlState &state, const FamilyState &family,
    const FamilyDeathState &deaths, const FamilyDeathOriginRead &death_read,
    const InteractionState &interactions, const InteractionAccess &access,
    FamilyLegacyState &legacy, const FamilyLegacyPlayableRead &playable,
    const StableId &actor, const CollateralControlCommand &cmd,
    const CollateralControlQuote &quote,
    const DiscipleControlRelationContext *disciple,
    const BiologicalControlDecisionContext *biological) {
  if (&c != &x.recognition.people.common || &state != &x.state)
    return {
        false, {}, common_failure("CollateralControlCandidateMismatch").error};
  auto fresh = preview_collateral_control_command(
      x, family, deaths, death_read, interactions, access, legacy, playable,
      actor, cmd, disciple, biological);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  FamilyEffect effect;
  effect.success = true;
  effect.player_actor_id = actor;
  if (cmd.kind == "family_collateral_recognize") {
    if (!quote.recognition || quote.decision || !fresh.value.recognition ||
        !collateral_recognition_source_equal(*quote.recognition,
                                             *fresh.value.recognition))
      return {
          false, {}, common_failure("StaleCollateralRecognitionSource").error};
    auto point = c.begin_savepoint();
    if (!point.ok)
      return {false, {}, point.error};
    auto old = state;
    auto abort = [&](const Status &error) -> Outcome<FamilyEffect> {
      auto back = c.rollback_savepoint(point.value);
      state = std::move(old);
      return {false, {}, back.ok ? error.error : back.error};
    };
    auto fact = record_collateral_recognition_in_candidate(
        c, x.recognition, state, *quote.recognition);
    if (!fact.ok)
      return abort({false, {}, fact.error});
    auto committed = c.commit_savepoint(point.value);
    if (!committed.ok)
      return abort(committed);
    effect.actor_id = actor;
    effect.fact_id = fact.value;
    return {true, std::move(effect), {}};
  }
  if (quote.recognition || !quote.decision || !fresh.value.decision ||
      !legacy_control_decision_equal(quote.decision->decision,
                                     fresh.value.decision->decision))
    return {false, {}, common_failure("StaleCollateralControlDecision").error};
  const auto &r = x.recognition;
  auto fact = record_family_legacy_decision(
      c, family, deaths, death_read, interactions, r.interactions, access,
      r.original_requests, legacy, *quote.decision, cmd.consent_fact_id,
      playable, disciple, biological, &x);
  if (!fact.ok)
    return {false, {}, fact.error};
  effect.fact_id = fact.value;
  effect.heir_actor_id = quote.decision->decision.successor_actor_id;
  return {true, std::move(effect), {}};
}
} // namespace hanmo::resident
