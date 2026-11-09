#include "private_shop_exact_service.hpp"
#include "private_shop_capability.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
const FixedServiceTransition& previous(const PrivateShopOperationSourceContext&x,const StableId&id){
 const FixedServiceTransition*out=nullptr;for(const auto&t:x.elapsed.fixed_service_transitions)if(t.action_ref==id&&(!out||t.transition_ordinal>out->transition_ordinal))out=&t;
 if(!out||out->transition_ordinal==std::numeric_limits<Integer>::max()||out->kind==FixedTransitionKind::terminal)fail("PrivateShopActualPredecessorRequired",id);return *out;
}
FixedServiceTransition private_shop_service_transition(
    const PrivateShopOperationSourceContext &x, const StableId &id,
    const StableId &actual_source, const WorldMoment &at) {
  const auto *entry = find_fixed_elapsed(x.elapsed, id);
  if (!entry || std::holds_alternative<ElapsedTerminal>(entry->timing) ||
      cmp(entry->last_settled_at, at) > 0)
    fail("PrivateShopActiveExactElapsedRequired", id);
  const auto &prior = previous(x, id);
  FixedServiceTransition out;
  out.transition_id = actual_source;
  out.action_ref = id;
  out.transition_ordinal = prior.transition_ordinal + 1;
  out.previous_transition_ref = prior.transition_id;
  out.previous_input_basis_refs = prior.input_basis_refs;
  out.occurred_at = at;
  out.credited_at = entry->credited_units;
  if (std::holds_alternative<ElapsedRunning>(entry->timing) &&
      cmp(entry->last_settled_at, at))
    out.credited_at =
        take(prepare_fixed_elapsed_slice(*entry, {entry->last_settled_at, at},
                                         x.scale, x.spatial.budget))
            .credited_units;
  out.execution_source_ref = private_shop_operation_started_source(id);
  out.execution_source_schema = 2;
  return out;
}
Status project(CommonStore&c,const PrivateShopOperationSourceContext&x,const StableId&id){
 NativePrivateShopActionTimeRead reader(x);const auto&r=private_shop_operation_action_time_registration();auto proof=take(reader.read_action_time_owner(r,id));
 const auto projection=take(read_action_time_projection(r,proof,x.elapsed,take(x.clock.read_current_moment()),x.scale,reader,x.spatial.budget));
 ActionMutation mutation;mutation.kind=ActionMutation::Kind::patch;mutation.action.id=id;mutation.remaining_ticks=projection.remaining_half_days;return c.apply_action(mutation);
}
}
Outcome<FixedServiceTransitionRef>
apply_private_shop_condition_change_in_candidate(
    CommonStore &c, ActionElapsedStore &elapsed,
    PrivateShopOperationCapabilitiesState &capabilities,
    const PrivateShopOperationSourceContext &x, const StableId &id,
    const MomentOwnerSource &source) {
  try {
    if (&c != &x.shops.common || &elapsed != &x.elapsed ||
        &capabilities != &x.capabilities || source.source_schema != 2 ||
        source.source_ref.domain != "actor_foundation" ||
        source.source_ref.kind != "condition_receipt")
      fail("PrivateShopConditionCandidateMismatch", id);
    const auto at = take(x.clock.read_current_moment());
    const auto changed =
        take(read_character_condition_source(x.body, source.source_ref, 2));
    const auto *operation = x.shops.state.operations.find(id);
    const auto *entry = find_fixed_elapsed(elapsed, id);
    if (!operation || !entry || !operation->result_fact_id.empty() ||
        !c.action(id) || changed.actor_id != operation->actor_id ||
        cmp(changed.occurred_at, at))
      fail("PrivateShopActualConditionBoundaryRequired", id);
    const auto current =
        take(read_current_character_condition(x.body, operation->actor_id));
    const auto current_source =
        take(read_character_condition_head_source(x.body, current.head, at));
    if (current_source.source_schema != 2 ||
        !same_source_ref(current_source.source, source.source_ref))
      fail("PrivateShopActualCurrentConditionRequired", id);
    const auto capability = take(read_character_capability_at_head(
        x.body, current.head,
        {"private_shop_operation", 1, x.body.catalog.identity.content_digest},
        std::nullopt, at));
    auto transition =
        private_shop_service_transition(x, id, source.source_ref.id, at);
    const bool running = std::holds_alternative<ElapsedRunning>(entry->timing);
    if (!running && !std::holds_alternative<ElapsedSuspended>(entry->timing))
      fail("PrivateShopActualServicePhaseRequired", id);
    transition.kind = running && !capability.allowed
                          ? FixedTransitionKind::suspended
                          : (!running && capability.allowed
                                 ? FixedTransitionKind::resumed
                                 : FixedTransitionKind::input_rebound);
    transition.cause = FixedEligibilityChanged{source.source_ref, 2,
                                               capability.rule_ref.id, id};
    transition.input_basis_refs =
        take(read_private_shop_current_service_inputs(x, id));
    NativePrivateShopActionTimeRead reader(x);
    const auto &r = private_shop_operation_action_time_registration();
    auto prepared = take(prepare_fixed_service_change(
        elapsed, id, transition,
        transition.kind == FixedTransitionKind::suspended
            ? std::optional<StableId>{source.source_ref.id}
            : std::nullopt,
        r, x.scale, reader, x.spatial.budget));
    take(apply_fixed_service_change_in_candidate(
        elapsed, std::move(prepared), r, x.scale, reader, x.spatial.budget));
    if (transition.kind == FixedTransitionKind::resumed) {
      const FixedServiceTransition *stop = nullptr;
      for (const auto &t : elapsed.fixed_service_transitions)
        if (t.action_ref == id && t.kind == FixedTransitionKind::suspended &&
            t.transition_ordinal < transition.transition_ordinal &&
            (!stop || t.transition_ordinal > stop->transition_ordinal))
          stop = &t;
      if (!stop)
        fail("PrivateShopActualSuspensionRequired", id);
      take(adopt_private_shop_stopped_capabilities_in_candidate(
          capabilities, x, {stop->transition_id, stop->source_schema}));
    }
    take(project(c, x, id));
    return {true, {transition.transition_id, transition.source_schema}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status adopt_private_shop_terminal_time_in_candidate(
    ActionElapsedStore &elapsed, const PrivateShopOperationSourceContext &x,
    const MomentOwnerSource &source) {
  try {
    if (&elapsed != &x.elapsed)
      fail("PrivateShopTerminalCandidateMismatch", source.source_ref.id);
    const auto &terminal =
        *take(read_original_private_shop_operation_terminal(x, source));
    const auto at = take(x.clock.read_current_moment());
    if (cmp(terminal.occurred_at, at))
      fail("PrivateShopActualTerminalBoundaryRequired", terminal.operation_ref);
    const auto *result =
        x.shops.state.operating_results.find(terminal.original_result_ref.id);
    if (!result)
      fail("PrivateShopActualTerminalResultRequired", terminal.operation_ref);
    auto transition = private_shop_service_transition(x, terminal.operation_ref,
                                                      result->id, at);
    transition.kind = FixedTransitionKind::terminal;
    transition.cause =
        FixedOwnerTerminated{source.source_ref, source.source_schema};
    transition.terminal_disposition =
        result->completed ? FixedTerminalDisposition::completed
                          : (result->reason == "cancelled"
                                 ? FixedTerminalDisposition::cancelled
                                 : FixedTerminalDisposition::interrupted);
    NativePrivateShopActionTimeRead reader(x);
    const auto &r = private_shop_operation_action_time_registration();
    auto prepared = take(prepare_fixed_service_change(
        elapsed, terminal.operation_ref, transition, std::nullopt, r, x.scale,
        reader, x.spatial.budget));
    return apply_fixed_service_change_in_candidate(
        elapsed, std::move(prepared), r, x.scale, reader, x.spatial.budget);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
}
