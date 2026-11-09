#include "resident/kernel/catalog_section_observation.hpp"
#include "control_continuation.hpp"
#include "control_retirement_cancel.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <tuple>
namespace hanmo::resident {
namespace {
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return std::tie(a.world_id, a.execution_epoch, a.revision, a.tick) ==
         std::tie(b.world_id, b.execution_epoch, b.revision, b.tick);
}
bool allocated(const StableId &id, const char *prefix, Integer next) {
  const std::string_view text = id, p = prefix;
  if (text.substr(0, p.size()) != p)
    return false;
  auto digits = text.substr(p.size());
  Integer n = 0;
  auto v = std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return v.ec == std::errc{} && v.ptr == digits.data() + digits.size() &&
         n > 0 && n < next && std::to_string(n) == digits;
}
Status broken(const StableId &code, const StableId &at = {}) {
  return common_failure(code, at, ErrorKind::internal);
}
Status boundary_history(const CommonStore &, const ControlContinuationState &,
                        const ContinuationOriginalSourceRead &);
Status same_candidate(const CommonStore &c,
                      const ContinuationFoundationSourceRead &s) {
  return s.bound_to(c) ? common_success()
                       : broken("ContinuationCandidateMismatch");
}
Status initial_and_chain(const CommonStore &c,
                         const ControlContinuationState &state,
                         const ContinuationFoundationSourceRead &sources) {
  auto valid = same_candidate(c, sources);
  if (!valid.ok)
    return valid;
  StableId actor =
      state.activation_records.empty()
          ? state.control_binding.actor_id
          : state.activation_records.rows().front().second.predecessor_actor_id;
  valid = sources.validate_initial_control_actor(actor);
  if (!valid.ok)
    return valid;
  Integer epoch = 0;
  for (const auto &row : state.activation_records.rows()) {
    const auto &a = row.second;
    if (row.first != a.activation_id ||
        !allocated(a.activation_id,
                   "control_activation:", c.state().next_instance_id) ||
        a.previous_control_epoch != epoch ||
        epoch == std::numeric_limits<Integer>::max() ||
        a.new_control_epoch != epoch + 1 || a.predecessor_actor_id != actor ||
        a.successor_actor_id == actor || !c.actor(a.successor_actor_id))
      return broken("InvalidControlActivationChain", row.first);
    const auto *h = c.state().facts.find(row.first);
    if (!h || h->domain != "control_continuation" ||
        h->kind != "control_activated")
      return broken("InvalidControlActivationSource", row.first);
    if (epoch > 0) {
      const ControlActivationRecord *previous = nullptr;
      for (const auto &old : state.activation_records.rows())
        if (old.second.new_control_epoch == epoch)
          previous = &old.second;
      if (!previous || a.committed_revision <= previous->committed_revision ||
          a.effective_tick < previous->effective_tick)
        return broken("InvalidControlActivationOrder", row.first);
      const auto &order = c.state().facts.order();
      if (std::find(order.begin(), order.end(), previous->activation_id) >=
          std::find(order.begin(), order.end(), a.activation_id))
        return broken("InvalidControlActivationOrder", row.first);
    }
    actor = a.successor_actor_id;
    epoch = a.new_control_epoch;
  }
  return state.control_binding.actor_id == actor &&
                 state.control_binding.control_epoch == epoch &&
                 c.state().metadata.player_actor_id == actor
             ? common_success()
             : broken("InvalidControlBinding");
}
// 原Pending共有身份核验；死后地基与完整接续沿同一算法，不另存控制账。
Status pending_identity(const CommonStore &c, const PendingContinuation &p,
                        const ContinuationFoundationSourceRead &sources) {
  auto now = sources.actual_now();
  if (!now.ok)
    return {false, {}, now.error};
  if (!allocated(p.pending_id, "continuation:", c.state().next_instance_id) ||
      !c.actor(p.predecessor_actor_id) || p.created_tick < 0 ||
      p.created_tick > now.value.half_day_tick || p.generation < 1 ||
      p.control_epoch_at_open < 0)
    return broken("InvalidPendingContinuation", p.pending_id);
  auto control = sources.validate_pending_control_origin(p);
  if (!control.ok)
    return control;
  if (p.status != ContinuationStatus::open &&
      p.status != ContinuationStatus::activated &&
      p.status != ContinuationStatus::cancelled &&
      p.status != ContinuationStatus::terminal_retained)
    return broken("InvalidContinuationStatus", p.pending_id);
  return common_success();
}
// 地基接口只反读原死亡来源；本机械拆分仍保原精确时刻条件，不扩大接受域。
Status pending_source(const CommonStore &c, const PendingContinuation &p,
                      const ContinuationFoundationSourceRead &sources) {
  auto valid = pending_identity(c, p, sources);
  if (!valid.ok)
    return valid;
  if (p.mode != ContinuationMode::after_death)
    return broken("ContinuationDeathRefRequired", p.pending_id);
  if (p.source_ref.domain != "family_life" ||
      p.source_ref.kind != "family_death" || p.source_ref.ordinal)
    return broken("ContinuationDeathRefRequired", p.pending_id);
  auto death = sources.read_death(
      {p.source_ref.domain, p.source_ref.kind, p.source_ref.fact_id});
  if (!death.ok)
    return {false, {}, death.error};
  if (death.value.actor_id != p.predecessor_actor_id ||
      death.value.recorded_at.kind != ActorFoundationAt::Kind::exact_moment ||
      death.value.recorded_at.exact_moment.half_day_tick != p.created_tick ||
      p.status == ContinuationStatus::cancelled)
    return broken("InvalidContinuationDeathSource", p.pending_id);
  return common_success();
}
// 完整入口继续保原生前准备及其来源，死后分支只委托上述唯一核验。
Status pending_source(const CommonStore &c, const PendingContinuation &p,
                      const ContinuationOriginalSourceRead &sources) {
  if (p.mode == ContinuationMode::after_death)
    return pending_source(
        c, p, static_cast<const ContinuationFoundationSourceRead &>(sources));
  auto valid = pending_identity(c, p, sources);
  if (!valid.ok)
    return valid;
  if (p.mode != ContinuationMode::voluntary_retirement)
    return broken("InvalidContinuationMode", p.pending_id);
  return sources.validate_voluntary_preparation(p);
}
Status candidate_inputs(const CommonStore &c,
                        const ControlContinuationState &state,
                        const StableId &pending_id,
                        const ContinuationCandidateRef &ref,
                        Integer expected_control_epoch,
                        const std::optional<bool> &voluntary_confirmation,
                        const ContinuationOriginalSourceRead &sources,
                        bool after_control) {
  auto valid = same_candidate(c, sources);
  if (!valid.ok)
    return valid;
  const auto *pending = state.pending_continuations.find(pending_id);
  if (!pending)
    return common_failure("NoContinuationPending");
  if (pending->status != ContinuationStatus::open)
    return common_failure("ContinuationAlreadyResolved");
  if (ref.pending_id != pending->pending_id ||
      ref.pending_generation != pending->generation)
    return common_failure("StaleContinuationGeneration");
  if (expected_control_epoch != pending->control_epoch_at_open ||
      ref.control_epoch != pending->control_epoch_at_open)
    return common_failure("StaleControlEpoch");
  if (!identity(ref.source_identity, c.state().identity))
    return common_failure("StalePreview");
  if (!after_control &&
      (state.control_binding.control_epoch != pending->control_epoch_at_open ||
       state.control_binding.actor_id != pending->predecessor_actor_id ||
       c.state().metadata.player_actor_id != pending->predecessor_actor_id))
    return common_failure("StaleControlEpoch");
  if (voluntary_confirmation &&
      *voluntary_confirmation !=
          (pending->mode == ContinuationMode::voluntary_retirement))
    return common_failure("ContinuationConfirmationMismatch");
  valid = pending_source(c, *pending, sources);
  if (!valid.ok)
    return valid;
  return sources.validate_current_candidate(*pending, ref);
}
bool same_pending(const PendingContinuation &a, const PendingContinuation &b) {
  return std::tie(a.pending_id, a.mode, a.predecessor_actor_id, a.created_tick,
                  a.generation, a.status, a.control_epoch_at_open) ==
             std::tie(b.pending_id, b.mode, b.predecessor_actor_id,
                      b.created_tick, b.generation, b.status,
                      b.control_epoch_at_open) &&
         same_continuation_cause(a.source_ref, b.source_ref) &&
         a.boundary_work.has_value() == b.boundary_work.has_value() &&
         (!a.boundary_work ||
          same_continuation_boundary_work(*a.boundary_work, *b.boundary_work));
}
} // namespace
bool same_continuation_cause(const CauseRef &a, const CauseRef &b) noexcept {
  return std::tie(a.domain, a.kind, a.fact_id, a.ordinal) ==
         std::tie(b.domain, b.kind, b.fact_id, b.ordinal);
}
bool same_continuation_consent(const ConsentRef &a,
                               const ConsentRef &b) noexcept {
  return std::tie(a.proposal_id, a.terms_version, a.decision_actor_id,
                  a.accepted_fact_id) ==
         std::tie(b.proposal_id, b.terms_version, b.decision_actor_id,
                  b.accepted_fact_id);
}
Outcome<ControlContinuationCatalog>
decode_control_continuation_catalog(const BoundaryValue &value) {
  try {
    using namespace boundary;
    fields(value, {"profile"}, {}, "control_continuation");
    const auto profile = get_string(value, "profile", "");
    if (profile == "explicit_control_continuation_v2")
      return {true, {true, 2}, {}};
    if (profile == "explicit_control_continuation_v3")
      return {true, {true, 3}, {}};
    unsupported("UnsupportedControlContinuationProfile", "");
    return {false, {}, {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status
validate_control_continuation_catalog(const ControlContinuationCatalog &c,
                                      const CatalogIdentity &identity) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 30, "ControlContinuationCatalog", "validate_control_continuation_catalog", &c);

  if (!c.enabled || (c.selected_version != 2 && c.selected_version != 3))
    return common_failure("ExplicitControlContinuationRequired", {},
                          ErrorKind::unsupported_capability);
  const StableId *selected = nullptr;
  for (const auto &m : identity.module_versions)
    if (m.first == "control_continuation") {
      if (selected)
        return broken("DuplicateControlContinuationModule");
      selected = &m.second;
    }
  return selected && *selected == std::to_string(c.selected_version)
             ? common_success()
             : common_failure("ControlContinuationSelectionMismatch", {},
                              ErrorKind::unsupported_capability);
}
Status initialize_control_continuation_in_candidate(
    const CommonStore &c, ControlContinuationState &state,
    const ControlContinuationCatalog &catalog,
    const CatalogIdentity &identity) {
  auto valid = validate_control_continuation_catalog(catalog, identity);
  if (!valid.ok)
    return valid;
  if (!state.control_binding.actor_id.empty() ||
      state.control_binding.control_epoch != 0 ||
      !state.pending_continuations.empty() ||
      !state.activation_records.empty() ||
      !state.continuation_consent_sources.empty() ||
      !c.state().history.empty() || !c.state().results.empty() ||
      !c.actor(c.state().metadata.player_actor_id))
    return broken("ControlContinuationFreshSeedRequired");
  state.control_binding = {c.state().metadata.player_actor_id, 0};
  return common_success();
}
Status
validate_control_continuation(const CommonStore &c,
                              const ControlContinuationState &state,
                              const ControlContinuationCatalog &catalog,
                              const CatalogIdentity &catalog_id,
                              const ContinuationOriginalSourceRead &sources) {
  auto valid = validate_control_continuation_catalog(catalog, catalog_id);
  if (!valid.ok)
    return valid;
  valid = initial_and_chain(c, state, sources);
  if (!valid.ok)
    return valid;
  auto now = sources.actual_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto controlled_death =
      sources.read_actor_death(state.control_binding.actor_id);
  if (!controlled_death.ok)
    return {false, {}, controlled_death.error};
  if (controlled_death.value) {
    std::size_t open = 0;
    for (const auto &row : state.pending_continuations.rows())
      if (row.second.mode == ContinuationMode::after_death &&
          (row.second.status == ContinuationStatus::open ||
           row.second.status == ContinuationStatus::terminal_retained) &&
          row.second.predecessor_actor_id == state.control_binding.actor_id &&
          row.second.source_ref.fact_id == controlled_death.value->death_ref.id)
        ++open;
    if (open != 1)
      return broken("ControlledDeathContinuationCoverageMismatch");
  }
  for (const auto &row : state.pending_continuations.rows()) {
    const auto &p = row.second;
    if (row.first != p.pending_id)
      return broken("InvalidPendingContinuation", row.first);
    if (p.boundary_work) {
      if (catalog.selected_version != 3)
        return common_failure("ContinuationBoundaryWorkRequiresControl3",
                              row.first, ErrorKind::unsupported_capability);
      valid = validate_continuation_boundary_work_shape(*p.boundary_work);
      if (!valid.ok)
        return valid;
    }
    valid = pending_source(c, p, sources);
    if (!valid.ok)
      return valid;
    std::size_t activation_count = 0;
    for (const auto &a : state.activation_records.rows())
      if (a.second.pending_id == p.pending_id)
        ++activation_count;
    if ((p.status == ContinuationStatus::activated && activation_count != 1) ||
        (p.status != ContinuationStatus::activated && activation_count != 0))
      return broken("ContinuationActivationCoverageMismatch", row.first);
    if (p.status == ContinuationStatus::open ||
        p.status == ContinuationStatus::terminal_retained) {
      if (p.predecessor_actor_id != state.control_binding.actor_id ||
          p.control_epoch_at_open != state.control_binding.control_epoch)
        return broken("ContinuationOpenControlMismatch", row.first);
      for (const auto &other : state.pending_continuations.rows())
        if (other.first != row.first &&
            (other.second.status == ContinuationStatus::open ||
             other.second.status == ContinuationStatus::terminal_retained))
          return broken("DuplicateOpenContinuation", row.first);
    }
    for (const auto &other : state.pending_continuations.rows())
      if (other.first != row.first &&
          same_continuation_cause(other.second.source_ref, p.source_ref) &&
          other.second.predecessor_actor_id == p.predecessor_actor_id &&
          other.second.mode == p.mode)
        return broken("DuplicateContinuationCause", row.first);
  }
  for (const auto &row : state.activation_records.rows()) {
    const auto &a = row.second;
    const auto *p = state.pending_continuations.find(a.pending_id);
    if (!p || p->status != ContinuationStatus::activated || p->mode != a.mode ||
        p->predecessor_actor_id != a.predecessor_actor_id ||
        p->control_epoch_at_open != a.previous_control_epoch ||
        !same_continuation_cause(p->source_ref, a.source_ref) ||
        a.command_result_ref.empty() ||
        a.request_origin.world_id != c.state().identity.world_id ||
        a.request_origin.execution_epoch_at_commit < 1 ||
        a.request_origin.request_seq < 1 ||
        a.request_origin.predecessor_actor_id != a.predecessor_actor_id ||
        a.command_kind != "select_continuation" ||
        a.committed_tick != a.effective_tick ||
        a.effective_tick < p->created_tick ||
        a.effective_tick > now.value.half_day_tick ||
        a.committed_revision < 1 || a.committed_revision > c.revision())
      return broken("InvalidControlActivationOrigin", row.first);
    for (const auto &other : state.activation_records.rows())
      if (other.first != row.first &&
          ((other.second.request_origin.world_id == a.request_origin.world_id &&
            other.second.request_origin.predecessor_actor_id ==
                a.request_origin.predecessor_actor_id &&
            other.second.request_origin.request_seq ==
                a.request_origin.request_seq) ||
           other.second.command_result_ref == a.command_result_ref))
        return broken("DuplicateControlActivationOrigin", row.first);
    valid = sources.validate_historical_activation(*p, a);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : state.continuation_consent_sources.rows()) {
    if (row.first != row.second.original_source.fact_id)
      return broken("InvalidContinuationConsentIndex", row.first);
    valid = sources.validate_consent_source(row.second);
    if (!valid.ok)
      return valid;
  }
  valid = sources.validate_original_consent_registry(state);
  if (!valid.ok)
    return valid;
  for (const auto &row : c.state().facts.rows())
    if (row.second.domain == "control_continuation" &&
        (row.second.kind != "control_activated" ||
         !state.activation_records.find(row.first)))
      return broken("MissingControlContinuationSource", row.first);
  if (catalog.selected_version == 3)
    return boundary_history(c, state, sources);
  for (const auto &row : c.state().results.rows())
    if (row.second && (row.second->kind == "stabilize_continuation_boundary" ||
                       row.second->kind == "cancel_continuation_boundary" ||
                       (row.second->details &&
                        (std::holds_alternative<ContinuationBoundaryReceipt>(
                             row.second->details->data) ||
                         std::holds_alternative<ContinuationBoundaryCancelled>(
                             row.second->details->data)))))
      return common_failure("ContinuationBoundaryReceiptRequiresControl3",
                            row.first, ErrorKind::unsupported_capability);
  return common_success();
}
Status validate_control_continuation_v2(
    const CommonStore &c, const ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources) {
  if (catalog.selected_version != 2)
    return common_failure("ControlContinuation2Required", {},
                          ErrorKind::unsupported_capability);
  return validate_control_continuation(c, state, catalog, identity, sources);
}
Status validate_control_continuation_v3(
    const CommonStore &c, const ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources) {
  if (catalog.selected_version != 3)
    return common_failure("ControlContinuation3Required", {},
                          ErrorKind::unsupported_capability);
  return validate_control_continuation(c, state, catalog, identity, sources);
}
Outcome<std::optional<StableId>> open_after_death_continuation_in_candidate(
    CommonStore &c, ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationFoundationSourceRead &sources, const DeathRef &ref) {
  auto valid = validate_control_continuation_catalog(catalog, identity);
  if (valid.ok)
    valid = same_candidate(c, sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto death = sources.read_death(ref);
  if (!death.ok)
    return {false, {}, death.error};
  CauseRef cause{ref.domain, ref.kind, ref.id, {}};
  for (const auto &row : state.pending_continuations.rows())
    if (row.second.mode == ContinuationMode::after_death &&
        row.second.predecessor_actor_id == death.value.actor_id &&
        same_continuation_cause(row.second.source_ref, cause)) {
      valid = pending_source(c, row.second, sources);
      return valid.ok
                 ? Outcome<std::optional<StableId>>{true, row.first, {}}
                 : Outcome<std::optional<StableId>>{false, {}, valid.error};
    }
  if (death.value.actor_id != state.control_binding.actor_id)
    return {true, std::nullopt, {}};
  if (c.state().metadata.player_actor_id != state.control_binding.actor_id ||
      (!c.actor(death.value.actor_id) || c.actor(death.value.actor_id)->alive))
    return {false, {}, broken("ContinuationControlledDeathMismatch").error};
  for (const auto &row : state.pending_continuations.rows())
    if (row.second.status == ContinuationStatus::open ||
        row.second.status == ContinuationStatus::terminal_retained)
      return {false,
              {},
              common_failure("ContinuationDeathSupersessionContractRequired",
                             row.first, ErrorKind::unsupported_capability)
                  .error};
  if (death.value.recorded_at.kind != ActorFoundationAt::Kind::exact_moment)
    return {false, {}, broken("ContinuationExactDeathRequired").error};
  auto now = sources.actual_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto eq =
      compare_world_moments(now.value, death.value.recorded_at.exact_moment);
  if (!eq.ok || eq.value != 0)
    return {
        false, {}, broken("ContinuationActualDeathTransactionRequired").error};
  auto id = c.next_id("continuation");
  if (!id.ok)
    return {false, {}, id.error};
  PendingContinuation p{id.value,
                        ContinuationMode::after_death,
                        death.value.actor_id,
                        cause,
                        now.value.half_day_tick,
                        1,
                        ContinuationStatus::open,
                        state.control_binding.control_epoch};
  valid = pending_source(c, p, sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!state.pending_continuations.insert(id.value, std::move(p)))
    return {false, {}, broken("DuplicatePendingContinuation").error};
  return {true, id.value, {}};
}
Outcome<StableId> open_voluntary_continuation_in_candidate(
    CommonStore &c, ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources,
    const CauseRef &preparation) {
  auto valid = validate_control_continuation_catalog(catalog, identity);
  if (valid.ok)
    valid = same_candidate(c, sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto now = sources.actual_now();
  if (!now.ok)
    return {false, {}, now.error};
  if (state.control_binding.actor_id != c.state().metadata.player_actor_id ||
      !c.actor(state.control_binding.actor_id) ||
      !c.actor(state.control_binding.actor_id)->alive)
    return {false,
            {},
            common_failure("ContinuationLivingPredecessorRequired").error};
  for (const auto &row : state.pending_continuations.rows()) {
    if (row.second.mode == ContinuationMode::voluntary_retirement &&
        same_continuation_cause(row.second.source_ref, preparation)) {
      valid = sources.validate_voluntary_preparation(row.second);
      return valid.ok ? Outcome<StableId>{true, row.first, {}}
                      : Outcome<StableId>{false, {}, valid.error};
    }
    if (row.second.status == ContinuationStatus::open ||
        row.second.status == ContinuationStatus::terminal_retained)
      return {false, {}, common_failure("ContinuationAlreadyPending").error};
  }
  PendingContinuation p;
  p.mode = ContinuationMode::voluntary_retirement;
  p.predecessor_actor_id = state.control_binding.actor_id;
  p.source_ref = preparation;
  p.created_tick = now.value.half_day_tick;
  p.control_epoch_at_open = state.control_binding.control_epoch;
  valid = sources.validate_voluntary_preparation(p);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto id = c.next_id("continuation");
  if (!id.ok)
    return {false, {}, id.error};
  p.pending_id = id.value;
  if (!state.pending_continuations.insert(id.value, std::move(p)))
    return {false, {}, broken("DuplicatePendingContinuation").error};
  return {true, id.value, {}};
}
Outcome<const PendingContinuation *> read_current_continuation_candidate(
    const CommonStore &c, const ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources,
    const ContinuationCandidateRef &ref) {
  auto valid = validate_control_continuation_catalog(catalog, identity);
  if (valid.ok)
    valid = candidate_inputs(c, state, ref.pending_id, ref, ref.control_epoch,
                             std::nullopt, sources, false);
  return valid.ok
             ? Outcome<const PendingContinuation *>{true,
                                                    state.pending_continuations
                                                        .find(ref.pending_id),
                                                    {}}
             : Outcome<const PendingContinuation *>{false, nullptr,
                                                    valid.error};
}
Status validate_select_continuation(
    const CommonStore &c, const ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources,
    const SelectContinuation &select) {
  auto valid = validate_control_continuation_catalog(catalog, identity);
  return valid.ok
             ? candidate_inputs(c, state, select.pending_id,
                                select.candidate_ref,
                                select.expected_control_epoch,
                                select.voluntary_confirmation, sources, false)
             : valid;
}
Status register_original_continuation_consent_in_candidate(
    const CommonStore &c, ControlContinuationState &state,
    const ContinuationOriginalSourceRead &sources,
    const ContinuationConsentSource &source) {
  auto valid = same_candidate(c, sources);
  if (valid.ok)
    valid = sources.validate_consent_source(source);
  if (!valid.ok)
    return valid;
  const auto *prior =
      state.continuation_consent_sources.find(source.original_source.fact_id);
  if (prior)
    return prior->decision == source.decision &&
                   same_continuation_consent(prior->consent_ref,
                                             source.consent_ref) &&
                   same_continuation_cause(prior->original_source,
                                           source.original_source)
               ? common_success()
               : broken("ConflictingContinuationConsentIndex");
  return state.continuation_consent_sources.insert(
             source.original_source.fact_id, source)
             ? common_success()
             : broken("DuplicateContinuationConsentIndex");
}
Outcome<StableId> record_control_activation_in_candidate(
    CommonStore &c, ControlContinuationState &state,
    const ControlContinuationState &before, const SelectContinuation &select,
    const ContinuationOriginalSourceRead &sources) {
  auto valid =
      candidate_inputs(c, before, select.pending_id, select.candidate_ref,
                       select.expected_control_epoch,
                       select.voluntary_confirmation, sources, true);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *prior = before.pending_continuations.find(select.pending_id);
  auto *pending = state.pending_continuations.find(select.pending_id);
  if (!pending || !same_pending(*prior, *pending) ||
      before.control_binding.actor_id != prior->predecessor_actor_id ||
      before.control_binding.control_epoch != prior->control_epoch_at_open ||
      prior->control_epoch_at_open == std::numeric_limits<Integer>::max() ||
      state.control_binding.control_epoch != prior->control_epoch_at_open + 1 ||
      state.control_binding.actor_id !=
          select.candidate_ref.candidate_actor_id ||
      c.state().metadata.player_actor_id != state.control_binding.actor_id ||
      state.activation_records.size() != before.activation_records.size())
    return {false, {}, broken("ControlAtomicRouteTransitionRequired").error};
  auto now = sources.actual_now();
  if (!now.ok)
    return {false, {}, now.error};
  if (now.value.half_day_tick != c.tick())
    return {false, {}, broken("ControlStableBoundaryRequired").error};
  auto id = c.next_id("control_activation");
  if (!id.ok)
    return {false, {}, id.error};
  ControlActivationRecord record;
  record.activation_id = id.value;
  record.pending_id = pending->pending_id;
  record.predecessor_actor_id = pending->predecessor_actor_id;
  record.successor_actor_id = state.control_binding.actor_id;
  record.path = select.candidate_ref.path;
  record.basis_ref = select.candidate_ref.basis_ref;
  record.acceptance_ref = select.candidate_ref.acceptance_ref;
  record.mode = pending->mode;
  record.source_ref = pending->source_ref;
  record.effective_tick = c.tick();
  record.previous_control_epoch = prior->control_epoch_at_open;
  record.new_control_epoch = state.control_binding.control_epoch;
  if (!state.activation_records.insert(id.value, std::move(record)))
    return {false, {}, broken("DuplicateControlActivation").error};
  valid =
      c.append_fact({id.value, "control_activated", "control_continuation"});
  if (!valid.ok)
    return {false, {}, valid.error};
  pending->status = ContinuationStatus::activated;
  return {true, id.value, {}};
}
Outcome<StableId> bind_control_activation_result_in_candidate(
    CommonStore &c, ControlContinuationState &state,
    const ControlContinuationState &before, const SelectContinuation &select,
    const CommitResult &result, const ContinuationOriginalSourceRead &sources) {
  auto valid = same_candidate(c, sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  ControlActivationRecord *target = nullptr;
  for (auto &row : state.activation_records.rows())
    if (!before.activation_records.find(row.first)) {
      if (target)
        return {false, {}, broken("MultipleControlActivationsInCommand").error};
      target = &row.second;
    }
  if (!target || target->pending_id != select.pending_id ||
      target->successor_actor_id != select.candidate_ref.candidate_actor_id ||
      !same_continuation_cause(target->basis_ref,
                               select.candidate_ref.basis_ref) ||
      !same_continuation_consent(target->acceptance_ref,
                                 select.candidate_ref.acceptance_ref) ||
      !result.ok || result.kind != "select_continuation" ||
      result.tick != target->effective_tick || result.revision != c.revision())
    return {false, {}, broken("ControlActivationCommitMismatch").error};
  auto actual = read_common_command_source(
      c, {"common", "command_result", result.result_id});
  if (!actual.ok)
    return {false, {}, actual.error};
  if (!actual.value.result || !actual.value.history ||
      actual.value.result->result_id != result.result_id ||
      actual.value.history->actor_id != target->predecessor_actor_id ||
      actual.value.history->kind != result.kind ||
      actual.value.history->tick != result.tick)
    return {false, {}, broken("ControlActivationCommitMismatch").error};
  ControlRequestOrigin origin{
      c.state().identity.world_id, c.state().identity.execution_epoch,
      target->predecessor_actor_id, actual.value.request_seq};
  if (!target->command_result_ref.empty()) {
    if (target->command_result_ref != result.result_id ||
        target->command_kind != result.kind ||
        target->committed_tick != result.tick ||
        target->committed_revision != result.revision ||
        target->request_origin.world_id != origin.world_id ||
        target->request_origin.execution_epoch_at_commit !=
            origin.execution_epoch_at_commit ||
        target->request_origin.predecessor_actor_id !=
            origin.predecessor_actor_id ||
        target->request_origin.request_seq != origin.request_seq)
      return {false, {}, broken("ConflictingControlActivationCommit").error};
    return {true, target->activation_id, {}};
  }
  target->command_result_ref = result.result_id;
  target->request_origin = std::move(origin);
  target->command_kind = result.kind;
  target->committed_tick = result.tick;
  target->committed_revision = result.revision;
  return {true, target->activation_id, {}};
}
Outcome<const ControlActivationRecord *> read_control_activation_for_replay(
    const CommonStore &c, const ControlContinuationState &state,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources, const StableId &predecessor,
    Integer seq) {
  if (seq < 1)
    return {false, {}, common_failure("InvalidRequestSequence").error};
  auto valid =
      validate_control_continuation(c, state, catalog, identity, sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  const ControlActivationRecord *match = nullptr;
  for (const auto &row : state.activation_records.rows())
    if (row.second.request_origin.world_id == c.state().identity.world_id &&
        row.second.request_origin.predecessor_actor_id == predecessor &&
        row.second.request_origin.request_seq == seq) {
      if (match)
        return {false, {}, broken("DuplicateControlActivationOrigin").error};
      match = &row.second;
    }
  if (!match)
    return {true, nullptr, {}};
  if (match->request_origin.execution_epoch_at_commit !=
      c.state().identity.execution_epoch)
    return {false, {}, common_failure("StaleWorld").error};
  return {true, match, {}};
}

namespace {
bool boundary_id(const StableId &value) {
  return !value.empty() && value.size() <= 256;
}
bool boundary_cause(const CauseRef &value) {
  return boundary_id(value.domain) && boundary_id(value.kind) &&
         boundary_id(value.fact_id) && (!value.ordinal || *value.ordinal >= 0);
}
bool boundary_consent(const ConsentRef &value) {
  return boundary_id(value.proposal_id) && value.terms_version >= 1 &&
         boundary_id(value.decision_actor_id) &&
         boundary_id(value.accepted_fact_id);
}
bool boundary_path(ContinuationPath value) {
  return value >= ContinuationPath::biological &&
         value <= ContinuationPath::disciple;
}
bool boundary_status(ContinuationBoundaryStatus value) {
  return value >= ContinuationBoundaryStatus::ready &&
         value <= ContinuationBoundaryStatus::fault;
}
bool boundary_owner(ContinuationBoundaryOwner value) {
  return value == ContinuationBoundaryOwner::military ||
         value == ContinuationBoundaryOwner::interaction;
}
bool boundary_source(const SourceRef &value) {
  return boundary_id(value.domain) && boundary_id(value.kind) &&
         boundary_id(value.id);
}
bool same_boundary_source(const SourceRef &a, const SourceRef &b) {
  return std::tie(a.domain, a.kind, a.id) == std::tie(b.domain, b.kind, b.id);
}
bool same_boundary_selection(const ContinuationCandidateRef &a,
                             const ContinuationCandidateRef &b) {
  return std::tie(a.pending_id, a.pending_generation, a.candidate_actor_id,
                  a.path, a.control_epoch) ==
             std::tie(b.pending_id, b.pending_generation, b.candidate_actor_id,
                      b.path, b.control_epoch) &&
         same_continuation_cause(a.basis_ref, b.basis_ref) &&
         same_continuation_consent(a.acceptance_ref, b.acceptance_ref);
}
Status boundary_sources(const std::vector<MomentOwnerSource> &values) {
  if (values.size() > 2)
    return broken("ContinuationBoundarySourceLimit");
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (!boundary_source(values[i].source_ref) || values[i].source_schema < 1)
      return broken("InvalidContinuationBoundarySource");
    for (std::size_t j = 0; j < i; ++j)
      if (same_boundary_source(values[i].source_ref, values[j].source_ref))
        return broken("DuplicateContinuationBoundarySource",
                      values[i].source_ref.id);
  }
  return common_success();
}
Status boundary_candidate_shape(const ContinuationCandidateRef &value) {
  bool world_nonzero = false;
  for (auto byte : value.source_identity.world_id)
    world_nonzero |= byte != 0;
  if (!boundary_id(value.pending_id) || value.pending_generation < 1 ||
      !boundary_id(value.candidate_actor_id) || !boundary_path(value.path) ||
      !boundary_cause(value.basis_ref) ||
      !boundary_consent(value.acceptance_ref) ||
      value.acceptance_ref.decision_actor_id != value.candidate_actor_id ||
      value.control_epoch < 0 || !world_nonzero ||
      value.source_identity.execution_epoch < 1 ||
      value.source_identity.revision < 0 || value.source_identity.tick < 0)
    return broken("InvalidContinuationCandidateRef", value.pending_id);
  return common_success();
}
} // namespace
Status validate_continuation_boundary_work_shape(
    const ContinuationBoundaryWork &value) {
  if (!boundary_id(value.candidate) || !boundary_path(value.path) ||
      !boundary_cause(value.basis_ref) ||
      !boundary_consent(value.acceptance_ref) ||
      value.acceptance_ref.decision_actor_id != value.candidate ||
      !boundary_owner(value.owner) ||
      !boundary_id(value.session_ref.session_id) ||
      value.session_ref.session_generation < 1 || value.frozen_ordinal < 0 ||
      !boundary_status(value.status))
    return broken("InvalidContinuationBoundaryWork");
  if (value.next_operation != ContinuationBoundaryOperation::resolve_frozen &&
      value.next_operation != ContinuationBoundaryOperation::close_terminal &&
      value.next_operation != ContinuationBoundaryOperation::resume_selected)
    return broken("InvalidContinuationBoundaryOperation");
  if ((value.owner == ContinuationBoundaryOwner::interaction) !=
      (value.next_operation == ContinuationBoundaryOperation::resume_selected))
    return broken("ContinuationBoundaryOwnerOperationMismatch");
  return boundary_sources(value.completed_source_refs);
}
bool same_continuation_boundary_work(
    const ContinuationBoundaryWork &a,
    const ContinuationBoundaryWork &b) noexcept {
  if (std::tie(a.candidate, a.path, a.owner, a.session_ref.session_id,
               a.session_ref.session_generation, a.frozen_ordinal,
               a.next_operation, a.status) !=
          std::tie(b.candidate, b.path, b.owner, b.session_ref.session_id,
                   b.session_ref.session_generation, b.frozen_ordinal,
                   b.next_operation, b.status) ||
      !same_continuation_cause(a.basis_ref, b.basis_ref) ||
      !same_continuation_consent(a.acceptance_ref, b.acceptance_ref) ||
      a.completed_source_refs.size() != b.completed_source_refs.size())
    return false;
  for (std::size_t i = 0; i < a.completed_source_refs.size(); ++i)
    if (a.completed_source_refs[i].source_schema !=
            b.completed_source_refs[i].source_schema ||
        !same_boundary_source(a.completed_source_refs[i].source_ref,
                              b.completed_source_refs[i].source_ref))
      return false;
  return true;
}
Status
validate_continuation_boundary_ref_shape(const ContinuationBoundaryRef &value) {
  auto valid = boundary_candidate_shape(value.candidate);
  if (!valid.ok)
    return valid;
  if (!boundary_owner(value.owner) || !boundary_id(value.session_ref) ||
      value.session_generation < 1 || value.frozen_segment_ordinal < 0 ||
      value.expected_next_boundary.valueless_by_exception())
    return broken("InvalidContinuationBoundaryRef");
  if (value.owner == ContinuationBoundaryOwner::military) {
    const auto *next = std::get_if<ContinuationMilitaryBoundary>(
        &value.expected_next_boundary);
    if (!next || (*next != ContinuationMilitaryBoundary::planning &&
                  *next != ContinuationMilitaryBoundary::result &&
                  *next != ContinuationMilitaryBoundary::closed))
      return broken("ContinuationBoundaryOwnerMismatch");
  } else {
    const auto *next =
        std::get_if<InteractionResponseNext>(&value.expected_next_boundary);
    if (!next || next->valueless_by_exception())
      return broken("ContinuationBoundaryOwnerMismatch");
    if (const auto *node = std::get_if<InteractionResponseNode>(next);
        node && !boundary_id(node->node_id))
      return broken("InvalidContinuationResponseNode");
  }
  return common_success();
}
Status validate_continuation_boundary_request_shape(
    const StabilizeContinuationBoundary &value) {
  auto valid = validate_continuation_boundary_ref_shape(value.boundary_ref);
  if (!valid.ok)
    return valid;
  const auto &chosen = value.boundary_ref.candidate;
  if (value.pending_id != chosen.pending_id ||
      value.expected_pending_generation != chosen.pending_generation ||
      value.candidate_actor_id != chosen.candidate_actor_id ||
      value.path != chosen.path ||
      !same_continuation_cause(value.basis_ref, chosen.basis_ref) ||
      !same_continuation_consent(value.acceptance_ref, chosen.acceptance_ref))
    return broken("ContinuationBoundarySelectionMismatch", value.pending_id);
  return common_success();
}
Status validate_continuation_boundary_receipt_shape(
    const ContinuationBoundaryReceipt &value) {
  auto valid = validate_continuation_boundary_request_shape(value.request);
  if (!valid.ok)
    return valid;
  const auto &prefix = value.candidate_prefix;
  if (prefix.common_fact_count < 0 ||
      bool(prefix.common_fact_count) != bool(prefix.last_common_fact) ||
      (prefix.last_common_fact &&
       (!boundary_id(prefix.last_common_fact->domain) ||
        !boundary_id(prefix.last_common_fact->kind) ||
        !boundary_id(prefix.last_common_fact->id))))
    return broken("InvalidContinuationCandidatePrefix");
  const auto &result = value.result;
  if (result.boundary_work_ref.domain != "common" ||
      result.boundary_work_ref.kind != "command_result" ||
      !boundary_id(result.boundary_work_ref.id) ||
      !boundary_status(result.status) || result.world_half_days != 0)
    return broken("InvalidContinuationBoundaryReceipt");
  auto moment = compare_world_moments(value.actual_at, value.actual_at);
  if (!moment.ok)
    return {false, {}, moment.error};
  if (value.actual_at.half_day_tick !=
      value.request.boundary_ref.candidate.source_identity.tick)
    return broken("ContinuationBoundaryAdvancedWorldTime");
  valid = boundary_sources(result.settled_source_refs);
  if (!valid.ok)
    return valid;
  if (result.next_boundary_ref) {
    const auto &next = *result.next_boundary_ref;
    const auto &before = value.request.boundary_ref;
    valid = validate_continuation_boundary_ref_shape(next);
    if (!valid.ok)
      return valid;
    if (!same_boundary_selection(before.candidate, next.candidate) ||
        next.owner != before.owner || next.session_ref != before.session_ref ||
        next.session_generation != before.session_generation ||
        next.candidate.source_identity.world_id !=
            before.candidate.source_identity.world_id ||
        next.candidate.source_identity.execution_epoch !=
            before.candidate.source_identity.execution_epoch ||
        before.candidate.source_identity.revision ==
            std::numeric_limits<Integer>::max() ||
        next.candidate.source_identity.revision !=
            before.candidate.source_identity.revision + 1 ||
        next.candidate.source_identity.tick !=
            before.candidate.source_identity.tick)
      return broken("ContinuationBoundaryNextSelectionMismatch");
  }
  return common_success();
}
Status validate_stabilize_continuation_candidate(
    const CommonStore &common, const ControlContinuationState &state,
    const ContinuationOriginalSourceRead &sources,
    const StabilizeContinuationBoundary &request) {
  auto valid = validate_continuation_boundary_request_shape(request);
  if (!valid.ok)
    return valid;
  const auto &candidate = request.boundary_ref.candidate;
  return candidate_inputs(common, state, request.pending_id, candidate,
                          candidate.control_epoch, std::nullopt, sources,
                          false);
}
Outcome<CurrentCommonFactPrefix> read_stabilize_continuation_candidate_prefix(
    const CommonStore &common, const ControlContinuationState &state,
    const ContinuationOriginalSourceRead &sources,
    const StabilizeContinuationBoundary &request) {
  auto valid = validate_stabilize_continuation_candidate(common, state, sources,
                                                         request);
  if (!valid.ok)
    return {false, {}, valid.error};
  return read_current_common_fact_prefix(common);
}
Status bind_continuation_boundary_receipt_seal_in_candidate(
    const CommonStore &common, ContinuationBoundaryReceipt &receipt,
    const CommitResult &seal) {
  auto valid = validate_continuation_boundary_request_shape(receipt.request);
  if (!valid.ok)
    return valid;
  const auto &origin = receipt.request.boundary_ref.candidate.source_identity;
  if (!seal.ok || seal.kind != "stabilize_continuation_boundary" ||
      !boundary_id(seal.result_id) ||
      origin.world_id != common.state().identity.world_id ||
      origin.execution_epoch != common.state().identity.execution_epoch ||
      origin.revision == std::numeric_limits<Integer>::max() ||
      seal.revision != origin.revision + 1 ||
      seal.revision != common.revision() || seal.tick != origin.tick ||
      seal.tick != common.tick())
    return broken("ContinuationBoundaryActualSealRequired");
  const auto &prior = receipt.result.boundary_work_ref;
  if ((!prior.domain.empty() || !prior.kind.empty() || !prior.id.empty()) &&
      (prior.domain != "common" || prior.kind != "command_result" ||
       prior.id != seal.result_id))
    return broken("ConflictingContinuationBoundarySeal");
  auto cut = read_actual_common_fact_prefix(common, receipt.candidate_prefix);
  if (!cut.ok)
    return {false, {}, cut.error};
  receipt.result.boundary_work_ref = {"common", "command_result",
                                      seal.result_id};
  return validate_continuation_boundary_receipt_shape(receipt);
}
Outcome<const ContinuationBoundaryReceipt *>
read_actual_continuation_boundary_receipt(const CommonStore &common,
                                          const SourceRef &source) {
  auto actual = read_common_command_source(common, source);
  if (!actual.ok)
    return {false, nullptr, actual.error};
  const auto &seal = *actual.value.result;
  if (seal.kind != "stabilize_continuation_boundary" || !seal.details)
    return {
        false, nullptr,
        broken("ContinuationBoundaryActualReceiptRequired", source.id).error};
  const auto *receipt =
      std::get_if<ContinuationBoundaryReceipt>(&seal.details->data);
  if (!receipt)
    return {
        false, nullptr,
        broken("ContinuationBoundaryActualReceiptRequired", source.id).error};
  auto valid = validate_continuation_boundary_receipt_shape(*receipt);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  auto cut = read_actual_common_fact_prefix(common, receipt->candidate_prefix);
  if (!cut.ok)
    return {false, nullptr, cut.error};
  const auto &chosen = receipt->request.boundary_ref.candidate;
  if (!same_boundary_source(receipt->result.boundary_work_ref, source) ||
      chosen.source_identity.world_id != common.state().identity.world_id ||
      chosen.source_identity.execution_epoch >
          common.state().identity.execution_epoch ||
      chosen.source_identity.revision == std::numeric_limits<Integer>::max() ||
      seal.revision != chosen.source_identity.revision + 1 ||
      seal.tick != chosen.source_identity.tick ||
      receipt->actual_at.half_day_tick != seal.tick)
    return {
        false, nullptr,
        broken("ContinuationBoundaryCommandLineageMismatch", source.id).error};
  return {true, receipt, {}};
}
Outcome<const ContinuationBoundaryReceipt *>
read_actual_continuation_boundary_receipt(
    const CommonStore &common, const ControlContinuationState &controls,
    const SourceRef &source) {
  auto receipt = read_actual_continuation_boundary_receipt(common, source);
  if (!receipt.ok)
    return receipt;
  auto actual = read_common_command_source(common, source);
  if (!actual.ok)
    return {false, nullptr, actual.error};
  const auto &request = receipt.value->request;
  const auto &chosen = request.boundary_ref.candidate;
  const auto *pending = controls.pending_continuations.find(request.pending_id);
  if (!pending || pending->pending_id != request.pending_id ||
      pending->predecessor_actor_id != actual.value.history->actor_id ||
      request.expected_pending_generation > pending->generation ||
      chosen.control_epoch != pending->control_epoch_at_open ||
      pending->created_tick > actual.value.result->tick)
    return {
        false, nullptr,
        broken("ContinuationBoundaryCommandLineageMismatch", source.id).error};
  return receipt;
}
Status validate_continuation_boundary_cancelled_shape(
    const ContinuationBoundaryCancelled &value) {
  const auto &r = value.request;
  if (!boundary_id(r.pending_id) || r.expected_control_epoch < 0 ||
      r.expected_pending_generation < 1 ||
      r.expected_pending_generation == std::numeric_limits<Integer>::max() ||
      value.generation != r.expected_pending_generation + 1)
    return broken("InvalidContinuationBoundaryCancellation");
  if (value.stopped_slice_ref &&
      (value.stopped_slice_ref->domain != "common" ||
       value.stopped_slice_ref->kind != "command_result" ||
       !boundary_id(value.stopped_slice_ref->id)))
    return broken("InvalidContinuationStoppedSlice");
  return common_success();
}
Outcome<PreparedContinuationBoundaryCancellation>
read_cancel_continuation_boundary(const CommonStore &common,
                                  const ControlContinuationState &controls,
                                  const ControlContinuationCatalog &catalog,
                                  const CatalogIdentity &identity,
                                  const ContinuationOriginalSourceRead &sources,
                                  const StableId &actor,
                                  const CancelContinuationBoundary &request) {
  auto fail = [](const Status &status)
      -> Outcome<PreparedContinuationBoundaryCancellation> {
    return {false, {}, status.error};
  };
  auto valid = validate_control_continuation_v3(common, controls, catalog,
                                                identity, sources);
  if (!valid.ok)
    return fail(valid);
  const auto *pending = controls.pending_continuations.find(request.pending_id);
  if (!pending)
    return fail(common_failure("NoContinuationPending"));
  if (pending->status != ContinuationStatus::open)
    return fail(common_failure("ContinuationAlreadyResolved"));
  if (actor != pending->predecessor_actor_id ||
      actor != controls.control_binding.actor_id ||
      actor != common.state().metadata.player_actor_id)
    return fail(common_failure("ContinuationPersonalCancellationRequired"));
  if (request.expected_control_epoch != pending->control_epoch_at_open ||
      request.expected_control_epoch != controls.control_binding.control_epoch)
    return fail(common_failure("StaleControlEpoch"));
  if (request.expected_pending_generation != pending->generation)
    return fail(common_failure("StaleContinuationGeneration"));
  if (pending->generation == std::numeric_limits<Integer>::max())
    return fail(broken("ContinuationGenerationOverflow"));
  const CommitResult *previous = nullptr;
  if (pending->boundary_work) {
    const auto &tail = *pending->boundary_work;
    for (const auto &row : common.state().results.rows())
      if (row.second && row.second->details)
        if (const auto *receipt = std::get_if<ContinuationBoundaryReceipt>(
                &row.second->details->data)) {
          const auto &r = receipt->request;
          const auto &b = r.boundary_ref;
          if (r.pending_id == pending->pending_id &&
              r.expected_pending_generation == pending->generation &&
              r.candidate_actor_id == tail.candidate && r.path == tail.path &&
              same_continuation_cause(r.basis_ref, tail.basis_ref) &&
              same_continuation_consent(r.acceptance_ref,
                                        tail.acceptance_ref) &&
              b.owner == tail.owner &&
              b.session_ref == tail.session_ref.session_id &&
              b.session_generation == tail.session_ref.session_generation &&
              (!previous || row.second->revision > previous->revision))
            previous = row.second.get();
        }
    if (!previous)
      return fail(broken("ContinuationBoundaryTailReceiptRequired",
                         pending->pending_id));
  }
  return {true, {pending, previous}, {}};
}
ProducedContinuationBoundaryCancelled::ProducedContinuationBoundaryCancelled(
    const ContinuationBoundaryCancelledBuildView &view)
    : copied_fields_([&] {


        return CheckpointMemoryCharge(0);
      }()),
      receipt_{{checkpoint_fresh_string_copy(view.request.pending_id),
                view.request.expected_control_epoch,
                view.request.expected_pending_generation},
               view.generation,
               view.stopped_slice_result_id
                   ? std::optional<SourceRef>{SourceRef{
                         "common", "command_result",
                         checkpoint_fresh_string_copy(
                             *view.stopped_slice_result_id)}}
                   : std::nullopt} {}
Outcome<ProducedContinuationBoundaryCancelled>
cancel_continuation_boundary_in_candidate(
    const CommonStore &common, ControlContinuationState &controls,
    const ControlContinuationCatalog &catalog, const CatalogIdentity &identity,
    const ContinuationOriginalSourceRead &sources, const StableId &actor,
    const CancelContinuationBoundary &request) {
  auto prepared = read_cancel_continuation_boundary(
      common, controls, catalog, identity, sources, actor, request);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  ProducedContinuationBoundaryCancelled produced{
      ContinuationBoundaryCancelledBuildView{
          request, prepared.value.pending->generation + 1,
          prepared.value.previous_slice
              ? &prepared.value.previous_slice->result_id
              : nullptr}};
  auto *pending = controls.pending_continuations.find(request.pending_id);
  pending->boundary_work.reset();
  pending->generation = produced.receipt().generation;
  // Nothing else changes: no owner/session mutation, world time, control
  // switch, reversal of a settled source, or cancellation of an original long
  // Action.
  return {true, std::move(produced), {}};
}
Outcome<const ContinuationBoundaryCancelled *>
read_actual_continuation_boundary_cancellation(
    const CommonStore &common, const ControlContinuationState &controls,
    const SourceRef &source) {
  auto actual = read_common_command_source(common, source);
  if (!actual.ok)
    return {false, nullptr, actual.error};
  const auto &seal = *actual.value.result;
  const auto *receipt =
      seal.details
          ? std::get_if<ContinuationBoundaryCancelled>(&seal.details->data)
          : nullptr;
  if (seal.kind != "cancel_continuation_boundary" || !receipt)
    return {false, nullptr,
            broken("ContinuationBoundaryCancellationReceiptRequired", source.id)
                .error};
  auto valid = validate_continuation_boundary_cancelled_shape(*receipt);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  const auto *pending =
      controls.pending_continuations.find(receipt->request.pending_id);
  if (!pending || pending->pending_id != receipt->request.pending_id ||
      pending->predecessor_actor_id != actual.value.history->actor_id ||
      pending->control_epoch_at_open !=
          receipt->request.expected_control_epoch ||
      receipt->generation > pending->generation ||
      seal.tick < pending->created_tick)
    return {false, nullptr,
            broken("ContinuationBoundaryCancellationLineageMismatch", source.id)
                .error};
  for (const auto &row : common.state().results.rows())
    if (row.second && row.second->result_id != seal.result_id &&
        row.second->details) {
      if (const auto *other = std::get_if<ContinuationBoundaryCancelled>(
              &row.second->details->data);
          other && other->request.pending_id == receipt->request.pending_id &&
          other->generation == receipt->generation)
        return {
            false, nullptr,
            broken("DuplicateContinuationGenerationSource", source.id).error};
      if (const auto *retired = std::get_if<PendingRetirementCancelled>(
              &row.second->details->data);
          retired && retired->pending_id == receipt->request.pending_id &&
          retired->generation == receipt->generation)
        return {
            false, nullptr,
            broken("DuplicateContinuationGenerationSource", source.id).error};
    }
  if (receipt->stopped_slice_ref) {
    auto stopped = read_actual_continuation_boundary_receipt(
        common, controls, *receipt->stopped_slice_ref);
    if (!stopped.ok)
      return {false, nullptr, stopped.error};
    auto prior =
        read_common_command_source(common, *receipt->stopped_slice_ref);
    if (!prior.ok)
      return {false, nullptr, prior.error};
    if (stopped.value->request.pending_id != receipt->request.pending_id ||
        stopped.value->request.expected_pending_generation !=
            receipt->request.expected_pending_generation ||
        prior.value.result->revision >= seal.revision)
      return {
          false, nullptr,
          broken("ContinuationBoundaryStoppedSliceMismatch", source.id).error};
  }
  return {true, receipt, {}};
}
Outcome<Integer> read_continuation_generation_at_revision(
    const CommonStore &common, const ControlContinuationState &controls,
    const StableId &pending_id, Integer revision) {
  auto fail = [&](const StableId &code) -> Outcome<Integer> {
    return {false, 0, broken(code, pending_id).error};
  };
  const auto *pending = controls.pending_continuations.find(pending_id);
  if (!pending || revision < 0 || revision > common.revision())
    return fail("ContinuationGenerationBoundaryRequired");
  Integer generation = 1, previous_revision = -1;
  while (true) {
    const CommitResult *next = nullptr;
    Integer next_generation = 0;
    for (const auto &row : common.state().results.rows())
      if (row.second && row.second->details &&
          row.second->revision > previous_revision &&
          row.second->revision <= revision) {
        Integer changed = 0;
        if (const auto *cancelled = std::get_if<ContinuationBoundaryCancelled>(
                &row.second->details->data);
            cancelled && cancelled->request.pending_id == pending_id)
          changed = cancelled->generation;
        if (const auto *retired = std::get_if<PendingRetirementCancelled>(
                &row.second->details->data);
            retired && retired->pending_id == pending_id)
          changed = retired->generation;
        if (changed && (!next || row.second->revision < next->revision)) {
          next = row.second.get();
          next_generation = changed;
        }
      }
    if (!next)
      break;
    SourceRef source{"common", "command_result", next->result_id};
    auto actual = read_common_command_source(common, source);
    if (!actual.ok)
      return {false, 0, actual.error};
    if (const auto *cancelled =
            std::get_if<ContinuationBoundaryCancelled>(&next->details->data)) {
      auto checked = read_actual_continuation_boundary_cancellation(
          common, controls, source);
      if (!checked.ok)
        return {false, 0, checked.error};
      if (cancelled->request.expected_pending_generation != generation)
        return fail("ContinuationGenerationSourceGap");
    } else {
      const auto &retired =
          std::get<PendingRetirementCancelled>(next->details->data);
      auto shape = validate_pending_retirement_cancelled_shape(retired);
      if (!shape.ok)
        return {false, 0, shape.error};
      if (next->kind != "cancel_pending_retirement" ||
          !same_continuation_cause(retired.original_withdrawn_preparation,
                                   pending->source_ref))
        return fail("ContinuationWithdrawalCommandMismatch");
    }
    if (generation == std::numeric_limits<Integer>::max() ||
        next_generation != generation + 1 ||
        next_generation > pending->generation ||
        actual.value.history->actor_id != pending->predecessor_actor_id ||
        next->tick < pending->created_tick)
      return fail("ContinuationGenerationSourceGap");
    generation = next_generation;
    previous_revision = next->revision;
  }
  return {true, generation, {}};
}
Status validate_original_continuation_boundary_receipt(
    const CommonStore &common, const ControlContinuationState &controls,
    const ContinuationOriginalSourceRead &sources, const SourceRef &source) {
  if (!sources.bound_to(common))
    return broken("ContinuationCandidateMismatch");
  auto actual =
      read_actual_continuation_boundary_receipt(common, controls, source);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto *pending =
      controls.pending_continuations.find(actual.value->request.pending_id);
  return sources.validate_historical_boundary_candidate(*pending,
                                                        *actual.value);
}
namespace {
Status boundary_history(const CommonStore &common,
                        const ControlContinuationState &controls,
                        const ContinuationOriginalSourceRead &sources) {
  for (const auto &row : controls.pending_continuations.rows()) {
    auto generation = read_continuation_generation_at_revision(
        common, controls, row.first, common.revision());
    if (!generation.ok)
      return {false, {}, generation.error};
    if (generation.value != row.second.generation)
      return broken("ContinuationGenerationSourceGap", row.first);
  }
  for (const auto &row : controls.pending_continuations.rows())
    if (row.second.status == ContinuationStatus::cancelled) {
      auto valid =
          sources.validate_historical_retirement_withdrawal(row.second);
      if (!valid.ok)
        return valid;
    }
  for (const auto &row : common.state().results.rows())
    if (row.second) {
      if (row.second->kind == "cancel_continuation_boundary" ||
          (row.second->details &&
           std::holds_alternative<ContinuationBoundaryCancelled>(
               row.second->details->data))) {
        auto actual = read_actual_continuation_boundary_cancellation(
            common, controls,
            {"common", "command_result", row.second->result_id});
        if (!actual.ok)
          return {false, {}, actual.error};
      }
      if (row.second->kind == "stabilize_continuation_boundary" ||
          (row.second->details &&
           std::holds_alternative<ContinuationBoundaryReceipt>(
               row.second->details->data))) {
        auto valid = validate_original_continuation_boundary_receipt(
            common, controls, sources,
            {"common", "command_result", row.second->result_id});
        if (!valid.ok)
          return valid;
      }
      if (row.second->kind == "cancel_pending_retirement" ||
          (row.second->details &&
           std::holds_alternative<PendingRetirementCancelled>(
               row.second->details->data))) {
        const auto *payload = row.second->details
                                  ? std::get_if<PendingRetirementCancelled>(
                                        &row.second->details->data)
                                  : nullptr;
        const auto *pending =
            payload ? controls.pending_continuations.find(payload->pending_id)
                    : nullptr;
        if (!pending)
          return broken("ContinuationWithdrawalPendingRequired", row.first);
        auto valid =
            sources.validate_historical_retirement_withdrawal(*pending);
        if (!valid.ok)
          return valid;
      }
    }
  for (const auto &row : controls.pending_continuations.rows())
    if (row.second.boundary_work) {
      const auto &tail = *row.second.boundary_work;
      bool actual_slice = false;
      for (const auto &item : common.state().results.rows())
        if (item.second && item.second->details)
          if (const auto *receipt = std::get_if<ContinuationBoundaryReceipt>(
                  &item.second->details->data)) {
            const auto &r = receipt->request;
            const auto &b = r.boundary_ref;
            if (r.pending_id == row.first &&
                r.expected_pending_generation == row.second.generation &&
                r.candidate_actor_id == tail.candidate && r.path == tail.path &&
                same_continuation_cause(r.basis_ref, tail.basis_ref) &&
                same_continuation_consent(r.acceptance_ref,
                                          tail.acceptance_ref) &&
                b.owner == tail.owner &&
                b.session_ref == tail.session_ref.session_id &&
                b.session_generation == tail.session_ref.session_generation)
              actual_slice = true;
          }
      if (!actual_slice)
        return broken("ContinuationBoundaryTailReceiptRequired", row.first);
      for (const auto &retained : tail.completed_source_refs) {
        bool found = false;
        for (const auto &item : common.state().results.rows())
          if (item.second && item.second->details)
            if (const auto *receipt = std::get_if<ContinuationBoundaryReceipt>(
                    &item.second->details->data)) {
              const auto &r = receipt->request;
              const auto &b = r.boundary_ref;
              if (r.pending_id != row.first ||
                  r.expected_pending_generation != row.second.generation ||
                  r.candidate_actor_id != tail.candidate ||
                  r.path != tail.path ||
                  !same_continuation_cause(r.basis_ref, tail.basis_ref) ||
                  !same_continuation_consent(r.acceptance_ref,
                                             tail.acceptance_ref) ||
                  b.owner != tail.owner ||
                  b.session_ref != tail.session_ref.session_id ||
                  b.session_generation != tail.session_ref.session_generation)
                continue;
              for (const auto &actual : receipt->result.settled_source_refs)
                if (actual.source_schema == retained.source_schema &&
                    same_boundary_source(actual.source_ref,
                                         retained.source_ref))
                  found = true;
            }
        if (!found)
          return broken("ContinuationBoundaryTailConsequenceRequired",
                        retained.source_ref.id);
      }
    }
  return common_success();
}
} // namespace
Status visit_continuation_pending_generation_source_refs(
    const CommonStore &common, const ControlContinuationState &controls,
    const StableId &pending_id, ContinuationBoundarySourceSink &sink) {
  auto generation = read_continuation_generation_at_revision(
      common, controls, pending_id, common.revision());
  if (!generation.ok)
    return {false, {}, generation.error};
  const auto *pending = controls.pending_continuations.find(pending_id);
  if (generation.value != pending->generation)
    return broken("ContinuationGenerationSourceGap", pending_id);
  for (const auto &row : common.state().results.rows())
    if (row.second && row.second->details) {
      const auto *boundary = std::get_if<ContinuationBoundaryCancelled>(
          &row.second->details->data);
      const auto *retired =
          std::get_if<PendingRetirementCancelled>(&row.second->details->data);
      if ((boundary && boundary->request.pending_id == pending_id) ||
          (retired && retired->pending_id == pending_id)) {
        SourceRef source{"common", "command_result", row.second->result_id};
        auto actual = read_common_command_source(common, source);
        if (!actual.ok)
          return {false, {}, actual.error};
        auto valid = sink.sealed_command(source);
        if (!valid.ok)
          return valid;
        if (boundary && boundary->stopped_slice_ref) {
          valid = sink.sealed_command(*boundary->stopped_slice_ref);
          if (!valid.ok)
            return valid;
        }
      }
    }
  return common_success();
}
Status visit_continuation_boundary_receipt_source_refs(
    const CommonStore &common, const ControlContinuationState &controls,
    const SourceRef &source, ContinuationBoundarySourceSink &sink) {
  auto actual =
      read_actual_continuation_boundary_receipt(common, controls, source);
  if (!actual.ok)
    return {false, {}, actual.error};
  auto valid = sink.sealed_command(actual.value->result.boundary_work_ref);
  if (!valid.ok)
    return valid;
  valid = sink.captured_candidate_prefix(actual.value->candidate_prefix);
  if (!valid.ok)
    return valid;
  // Cancellation changes generation without appending a Common fact. Its
  // original result chain is retained by the same pending owner, not a ledger.
  valid = visit_continuation_pending_generation_source_refs(
      common, controls, actual.value->request.pending_id, sink);
  if (!valid.ok)
    return valid;

  for (const auto &original : actual.value->result.settled_source_refs) {
    valid = sink.original_consequence(original);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
} // namespace hanmo::resident
