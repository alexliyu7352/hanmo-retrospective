#include "growth_fixed_control_inputs.hpp"
#include "growth_fixed_control_source.hpp"
#include "growth_fixed_start.hpp"
#include "growth_model.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
namespace {
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool same(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions &&
         a.artifact_sha256 == b.artifact_sha256;
}
Outcome<LifePlayerActorRoute>
original_route(const ProfileControlEligibilitySources &sources,
               const GrowthState &growth, const StableId &id,
               const StableId &actor) {
  auto control =
      read_profile_control_before_growth_boundary(sources, growth, id);
  if (!control.ok)
    return {false, {}, control.error};
  if (control.value.actor_id != actor || !sources.foundation_catalog)
    return {false,
            {},
            common_failure("GrowthFixedPlayerControlMismatch", id).error};
  LifePlayerActorRoute route;
  Status status = common_success();
  std::visit(
      [&](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AuthoredProfileController>)
          route.original_control_source_ref = LifeCatalogFieldRef{
              sources.foundation_catalog->identity, value.scenario_id,
              value.actor_id, "player_actor"};
        else {
          const StableId *source = nullptr;
          if constexpr (std::is_same_v<T, CreatedProfileController>)
            source = &value.creation_fact_id;
          else if constexpr (std::is_same_v<T, SuccessionProfileController>)
            source = &value.succession_fact_id;
          else if constexpr (std::is_same_v<T, ContinuationProfileController>)
            source = &value.activation_id;
          else
            source = &value.activation_fact_id;
          const auto *head = sources.common.state().facts.find(*source);
          if (!head) {
            status = common_failure("GrowthFixedActualControlSourceRequired",
                                    *source);
            return;
          }
          route.original_control_source_ref =
              SourceRef{head->domain, head->kind, *source};
        }
      },
      control.value.source);
  if (!status.ok)
    return {false, {}, status.error};
  return {true, std::move(route), {}};
}
Outcome<LifeNpcActorRoute> npc_route(const CharacterConditionSources &body,
                                     const LifeFixedServiceBoundary &boundary,
                                     const LifeActualCommand &command,
                                     const LifeNpcActorRoute *frozen) {
  if (!body.director || !body.director_catalog || !body.director->subject2)
    return {false,
            {},
            common_failure("GrowthFixedNpcActualDecisionReadRequired",
                           boundary.boundary_id)
                .error};
  const DirectorAdoptionResult2 *decision = nullptr;
  if (frozen) {
    const auto key = director_adoption_key2(
        {SubjectKind::actor, command.actor_id}, frozen->registration_epoch,
        frozen->decision_seq);
    decision = body.director->subject2->adoption_results.find(key);
  } else
    for (const auto &row : body.director->subject2->adoption_results.rows())
      if (row.second.accepted_command &&
          row.second.accepted_command->result_id ==
              command.command_binding.result_id) {
        if (decision)
          return {false,
                  {},
                  common_failure("GrowthFixedNpcCommandAdoptionAmbiguous",
                                 boundary.boundary_id)
                      .error};
        decision = &row.second;
      }
  if (!decision)
    return {false,
            {},
            common_failure("GrowthFixedNpcActualDecisionReadRequired",
                           boundary.boundary_id)
                .error};
  auto actual = read_director_accepted_command(
      body.common, *body.director, *body.director_catalog,
      body.catalog.identity,
      {command.actor_id, decision->registration_epoch, decision->decision_seq,
       command.command_binding.command_kind,
       command.command_binding.request_seq, command.command_binding.result_id});
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto *accepted = std::get_if<SourceRef>(&command.accepted_business_ref);
  const auto &source = *actual.value.command;
  auto at = compare_world_moments(source.accepted_at, boundary.occurred_at);
  if (!at.ok)
    return {false, {}, at.error};
  if (at.value || !accepted || source.actual_effect.source_schema != 1 ||
      !same(*accepted, source.actual_effect.source_ref))
    return {false,
            {},
            common_failure("GrowthFixedNpcActualBusinessMismatch",
                           boundary.boundary_id)
                .error};
  LifeNpcActorRoute result;
  result.subject_actor_id = command.actor_id;
  result.registration_epoch = decision->registration_epoch;
  result.decision_seq = decision->decision_seq;
  result.policy_ref = LifeCatalogFieldRef{
      source.policy_catalog, body.common.state().metadata.scenario_id,
      source.policy_id, "director_policy"};
  if (frozen) {
    const auto *stored = std::get_if<LifeCatalogFieldRef>(&frozen->policy_ref);
    const auto &expected = std::get<LifeCatalogFieldRef>(result.policy_ref);
    if (frozen->subject_actor_id != result.subject_actor_id || !stored ||
        !same(stored->catalog_identity, expected.catalog_identity) ||
        stored->scenario_id != expected.scenario_id ||
        stored->entity_id != expected.entity_id ||
        stored->field_selector != expected.field_selector)
      return {false,
              {},
              common_failure("GrowthFixedNpcOriginalPolicyMismatch",
                             boundary.boundary_id)
                  .error};
  }
  return {true, std::move(result), {}};
}
Status command(const CharacterConditionSources &body, const GrowthState &growth,
               const LifeFixedServiceBoundary &boundary,
               const LifeActualCommand &command) {
  if (!body.control || &body.control->common != &body.common ||
      body.control->foundation != &body.state ||
      body.control->foundation_catalog != &body.catalog)
    return common_failure("GrowthFixedControlSourcesRequired",
                          boundary.boundary_id);
  const auto &binding = command.command_binding;
  const auto *retained = body.common.state().results.find(
      command.actor_id + ":" + std::to_string(binding.request_seq));
  if (!retained || !*retained || !(*retained)->ok || !(*retained)->details ||
      (*retained)->result_id != binding.result_id ||
      (*retained)->kind != binding.command_kind ||
      (*retained)->tick != boundary.occurred_at.half_day_tick ||
      (*retained)->revision < 1 ||
      (*retained)->revision > body.common.revision())
    return common_failure("GrowthFixedRetainedActualCommandRequired",
                          boundary.boundary_id);
  const auto &result = **retained;
  std::size_t matched = 0;
  for (const auto &history : body.common.state().history)
    if (history.result_id == binding.result_id) {
      ++matched;
      if (history.actor_id != command.actor_id ||
          history.kind != binding.command_kind || history.tick != result.tick)
        return common_failure("GrowthFixedActualCommandHistoryMismatch",
                              boundary.boundary_id);
    }
  if (matched != 1)
    return common_failure("GrowthFixedRetainedActualHistoryRequired",
                          boundary.boundary_id);
  if (boundary.kind == LifeFixedBoundaryKind::started) {
    const auto *value = std::get_if<ActionStarted>(&result.details->data);
    if (!value || value->action_id != boundary.action_ref)
      return common_failure("GrowthFixedActualStartResultMismatch",
                            boundary.boundary_id);
    if (binding.command_kind != boundary.action_kind) {
      auto original = validate_growth_dialogue_confirmation(
          body.common, growth, body.interaction, body.interaction_catalog,
          binding.result_id, binding.request_seq, command.actor_id,
          boundary.action_kind, boundary.action_ref, boundary.occurred_at);
      if (!original.ok)
        return original;
    }
  } else {
    const auto *value = std::get_if<GrowthEffect>(&result.details->data);
    if (!value || value->kind != binding.command_kind ||
        value->action_id != boundary.action_ref)
      return common_failure("GrowthFixedActualControlResultMismatch",
                            boundary.boundary_id);
    if (boundary.kind == LifeFixedBoundaryKind::suspended && !value->paused)
      return common_failure("GrowthFixedActualPauseResultMismatch",
                            boundary.boundary_id);
    if (boundary.kind == LifeFixedBoundaryKind::resumed && value->paused)
      return common_failure("GrowthFixedActualResumeResultMismatch",
                            boundary.boundary_id);
  }
  if (const auto *terminal =
          std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop =
            std::get_if<LifeActualStopTrigger>(&terminal->trigger)) {
      if (binding.command_kind != "rest_cancel" ||
          !body.catalog.original_long_rest_rule ||
          !actor_foundation_rule_equal(
              stop->original_stop_rule_ref,
              body.catalog.original_long_rest_rule->rule_ref))
        return common_failure("GrowthFixedOriginalCommandStopRuleMismatch",
                              boundary.boundary_id);
    }

  const auto *player = std::get_if<LifePlayerActorRoute>(&command.route_basis);
  if (!player) {
    auto actual = npc_route(body, boundary, command,
                            &std::get<LifeNpcActorRoute>(command.route_basis));
    return actual.ok ? common_success() : Status{false, {}, actual.error};
  }
  auto actual = original_route(*body.control, growth, boundary.boundary_id,
                               command.actor_id);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &expected = actual.value.original_control_source_ref;
  const auto &stored = player->original_control_source_ref;
  if (expected.index() != stored.index())
    return common_failure("GrowthFixedControlSourceMismatch",
                          boundary.boundary_id);
  if (const auto *source = std::get_if<SourceRef>(&expected)) {
    if (!same(*source, std::get<SourceRef>(stored)))
      return common_failure("GrowthFixedControlSourceMismatch",
                            boundary.boundary_id);
  } else if (const auto *source = std::get_if<LifeCatalogFieldRef>(&expected)) {
    const auto &value = std::get<LifeCatalogFieldRef>(stored);
    if (!same(source->catalog_identity, value.catalog_identity) ||
        source->scenario_id != value.scenario_id ||
        source->entity_id != value.entity_id ||
        source->field_selector != value.field_selector)
      return common_failure("GrowthFixedControlSourceMismatch",
                            boundary.boundary_id);
  } else
    return common_failure("GrowthFixedControlSourceMismatch",
                          boundary.boundary_id);
  return common_success();
}
} // namespace
Outcome<LifePlayerActorRoute>
capture_growth_player_route(const ProfileControlEligibilitySources &sources,
                            const GrowthState &growth, const StableId &id,
                            const StableId &actor) {
  return original_route(sources, growth, id, actor);
}
Outcome<LifeActorRoute>
capture_growth_actor_route(const CharacterConditionSources &body,
                           const GrowthState &growth, const StableId &id,
                           const LifeActualCommand &command) {
  if (body.growth != &growth || !body.control ||
      &body.control->common != &body.common)
    return {false,
            {},
            common_failure("GrowthFixedControlSourcesRequired", id).error};
  const auto *boundary = growth.fixed_service_boundaries.find(id);
  if (!boundary)
    return {false,
            {},
            common_failure("GrowthFixedActualBoundaryRequired", id).error};
  auto control =
      read_profile_control_before_growth_boundary(*body.control, growth, id);
  if (!control.ok)
    return {false, {}, control.error};
  if (control.value.actor_id == command.actor_id) {
    auto player = original_route(*body.control, growth, id, command.actor_id);
    if (!player.ok)
      return {false, {}, player.error};
    return {true, LifeActorRoute{std::move(player.value)}, {}};
  }
  auto npc = npc_route(body, *boundary, command, nullptr);
  if (!npc.ok)
    return {false, {}, npc.error};
  return {true, LifeActorRoute{std::move(npc.value)}, {}};
}
// 复用唯一原 command/route 校验，只借真实所属边界与结果；此口不证明采用时序。
Outcome<const CommitResult *> read_growth_fixed_control_command_source(
    const CharacterConditionSources &body, const GrowthState &growth,
    const LifeFixedServiceBoundary &boundary) {
  if (body.growth != &growth ||
      growth.fixed_service_boundaries.find(boundary.boundary_id) != &boundary)
    return {false, nullptr,
            common_failure("GrowthFixedActualOwnedBoundaryRequired",
                           boundary.boundary_id)
                .error};
  const LifeActualCommand *actual =
      std::get_if<LifeActualCommand>(&boundary.cause);
  if (const auto *terminal =
          std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop =
            std::get_if<LifeActualStopTrigger>(&terminal->trigger))
      actual = std::get_if<LifeActualCommand>(&stop->source);
  if (!actual)
    return {false, nullptr,
            common_failure("GrowthFixedOriginalCommandSourceRequired",
                           boundary.boundary_id,
                           ErrorKind::unsupported_capability)
                .error};
  auto valid = command(body, growth, boundary, *actual);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  const auto *retained = body.common.state().results.find(
      actual->actor_id + ":" +
      std::to_string(actual->command_binding.request_seq));
  // command 已核同缓存行/result/history；同步只读期间不复制结果或建立第二来源。
  if (!retained || !*retained)
    return {false, nullptr,
            common_failure("GrowthFixedRetainedActualCommandRequired",
                           boundary.boundary_id)
                .error};
  return {true, retained->get(), {}};
}
Status
validate_growth_fixed_control_inputs(const CharacterConditionSources &body,
                                     const GrowthState &growth) {
  if (body.growth != &growth)
    return common_failure("GrowthFixedControlSameCandidateRequired");
  for (const auto &row : growth.fixed_service_boundaries.rows()) {
    const auto &boundary = row.second;
    const LifeActualCommand *actual =
        std::get_if<LifeActualCommand>(&boundary.cause);
    if (const auto *terminal =
            std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
      if (const auto *stop =
              std::get_if<LifeActualStopTrigger>(&terminal->trigger))
        actual = std::get_if<LifeActualCommand>(&stop->source);
    if (actual) {
      auto valid =
          read_growth_fixed_control_command_source(body, growth, boundary);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  }
  return common_success();
}
} // namespace hanmo::resident
