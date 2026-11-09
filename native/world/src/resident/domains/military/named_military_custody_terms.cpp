#include "resident/domains/military/military_model.hpp"
// 原俘获期限的唯一历史：写者与冷读共用同一来源核验，尚不开放到期释放。
#include "field_battle/field_segment_settlement.hpp"
#include "named_military_custody_terms.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
HalfDayDuration half_days(Integer ticks) {
  char text[std::numeric_limits<Integer>::digits10 + 2];
  const auto encoded = std::to_chars(text, text + sizeof(text), ticks);
  if (ticks <= 0 || encoded.ec != std::errc{})
    fail("NamedMilitaryCustodyPolicyRequired");
  return {take(
      normalize_fraction(std::string_view(text, encoded.ptr - text), "1"))};
}

bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool fields(const FieldOrder &actual, FieldOrder expected) {
  if (actual.empty())
    return true;
  auto copy = actual;
  std::sort(copy.begin(), copy.end());
  std::sort(expected.begin(), expected.end());
  return copy == expected;
}
std::size_t position(const NamedMilitaryCustodyContext &context,
                     const SourceRef &source) {
  return take(read_actual_common_fact_position(context.common, source));
}
Integer sequence(const StableId &id) {
  const StableId prefix = "named_military_custody:";
  if (!id.starts_with(prefix))
    return -1;
  const auto suffix = id.substr(prefix.size());
  Integer n = 0;
  const auto result =
      std::from_chars(suffix.data(), suffix.data() + suffix.size(), n);
  return result.ec == std::errc{} &&
                 result.ptr == suffix.data() + suffix.size() && n > 0 &&
                 std::to_string(n) == suffix
             ? n
             : -1;
}
// 作者规则引用不进入 Common；内容锁与实际 policy 由同 World 目录共同约束。
SourceRef policy_source(const NamedMilitaryCustodyPolicy &policy) {
  return {"military_catalog", "named_custody_policy", policy.rule_id};
}
// 这里只核期限区间的真实关闭头与顺序；关闭正文的完整空间/控制验证由 release
// validator 负责。
bool closed_before(const NamedMilitaryCustodyContext &context,
                   const NamedMilitaryCustodyTerm &term,
                   std::size_t observation) {
  const NamedMilitaryCustodyRelease *closure = nullptr;
  for (const auto &row : context.military.named_custody.closures)
    if (row.term_id == term.id) {
      if (closure || row.actor_id != term.actor_id)
        fail("NamedMilitaryCustodyReleaseRepeated", term.id);
      closure = &row;
    }
  if (!closure)
    return false;
  // 本版仅真实到期关闭；晚补的关闭不能倒作第六个半日的原事件。
  if (closure->recorded_tick != closure->released_at.half_day_tick ||
      take(compare_world_moments(closure->released_at, term.due_at)) != 0)
    fail("NamedMilitaryCustodyActualDeadlineRequired", closure->id);
  const auto at = position(
      context, {"military", "military_named_custody_released", closure->id});
  if (position(context, {"military", "military_named_custody_term", term.id}) >=
      at)
    fail("NamedMilitaryCustodyReleaseOrderMismatch", closure->id);
  return at < observation;
}
const NamedMilitaryCustodyTerm *
term_for_actor_at_capture(const NamedMilitaryCustodyContext &context,
                          const StableId &actor, const SourceRef &capture) {
  const auto observation = position(context, capture);
  const NamedMilitaryCustodyTerm *found = nullptr;
  for (const auto &term : context.military.named_custody.terms)
    if (term.actor_id == actor &&
        position(context, term.capture_source) <= observation &&
        !closed_before(context, term, observation)) {
      if (found)
        fail("NamedMilitaryCustodyOverlappingTerms", actor);
      found = &term;
    }
  return found;
}
// Only an actual Field segment closing can precede Common publication. This
// proof is borrowed from S0; neither tick+1 nor a caller-supplied M grants it.
void require_actual_field_receipt_closing(
    const NamedMilitaryCustodyContext &context,
    const MilitaryCasualtyFact &receipt) {
  if (!context.field || !context.field_applications || !context.clock ||
      !context.field_applications->bound_to(context.common, context.military,
                                            *context.field))
    fail("NamedMilitaryCustodyFieldClosingSourceRequired", receipt.id);
  const auto source = take(read_field_segment_settlement_source(
      context.common, context.military, *context.field,
      {"military", "field_battle_segment", receipt.battle_id},
      context.field_applications));
  const auto closing =
      take(context.field_applications->read_field_window_closing_moment());
  const auto now = take(context.clock->read_current_moment());
  if (source.casualties != &receipt || !source.settlement ||
      receipt.tick != source.settlement->applied_at.half_day_tick ||
      take(compare_world_moments(source.settlement->applied_at, closing)) !=
          0 ||
      take(compare_world_moments(now, closing)) != 0)
    fail("NamedMilitaryCustodyFieldClosingSourceRequired", receipt.id);
}
void require_actual_field_closing(const NamedMilitaryCustodyContext &context,
                                  const OriginalNamedMilitaryCapture &capture,
                                  Integer recorded_tick) {
  if (!context.field || !context.field_applications || !context.clock ||
      !capture.receipt || !capture.change || !capture.applied_at ||
      !context.field_applications->bound_to(context.common, context.military,
                                            *context.field))
    fail("NamedMilitaryCustodyFieldClosingSourceRequired");
  const auto actual = take(read_field_segment_captivity_source(
      context.common, context.military, *context.field,
      SourceRef{"military", "military_casualties", capture.receipt->id},
      capture.change->actor_id, *context.field_applications));
  const auto closing =
      take(context.field_applications->read_field_window_closing_moment());
  const auto now = take(context.clock->read_current_moment());
  if (actual.source.casualties != capture.receipt ||
      actual.actor_change != capture.change || !actual.source.settlement ||
      recorded_tick != capture.applied_at->half_day_tick ||
      take(compare_world_moments(actual.source.settlement->applied_at,
                                 *capture.applied_at)) != 0 ||
      take(compare_world_moments(closing, *capture.applied_at)) != 0 ||
      take(compare_world_moments(now, closing)) != 0)
    fail("NamedMilitaryCustodyFieldClosingSourceRequired", capture.receipt->id);
}

// pending 仅为本次真实收据，容许写入前尚未有它的期限；其它已发生俘获不能漏。
void validate_rows(const NamedMilitaryCustodyContext &context,
                   const MilitaryCasualtyFact *pending = nullptr) {
  take(validate_named_military_custody_policy(context));
  const auto &history = context.military.named_custody;
  if (!fields(history.field_order, {"terms", "closures"}))
    fail("NamedMilitaryCustodyHistoryShapeMismatch");
  const bool selected = take(selected_named_military_custody(context.identity));
  std::set<StableId> ids;
  std::map<StableId, const NamedMilitaryCustodyTerm *> previous_terms;
  std::set<std::pair<StableId, StableId>> captures;
  std::size_t previous_position = 0;
  Integer previous_sequence = 0, previous_tick = 0;
  const NamedMilitaryCustodyTerm *previous_term = nullptr;
  for (const auto &term : history.terms) {
    if (!selected)
      fail("UnexpectedNamedMilitaryCustodyHistory", term.id);
    const auto &policy = *context.catalog.named_custody_policy;
    const auto serial = sequence(term.id);
    if (serial <= previous_sequence ||
        serial >= context.common.state().next_instance_id ||
        term.actor_id.empty() || !ids.insert(term.id).second ||
        !captures.emplace(term.capture_source.id, term.actor_id).second ||
        term.capture_tick < 0 || term.recorded_tick != term.capture_tick ||
        term.recorded_tick < previous_tick || term.duration_half_days <= 0 ||
        term.duration_half_days >
            std::numeric_limits<Integer>::max() - term.capture_tick ||
        term.due_tick != term.capture_tick + term.duration_half_days ||
        term.duration_half_days != policy.duration_half_days ||
        !same(term.policy_source, policy_source(policy)) ||
        !fields(term.field_order,
                {"id", "actor_id", "captured_army_id",
                 "captured_organization_id", "capture_source",
                 "capture_application_source", "capture_actor_ordinal",
                 "capture_application_schema", "army_formation_source",
                 "army_formation_schema", "policy_source", "capture_tick",
                 "recorded_tick", "duration_half_days", "due_tick",
                 "adopted_at", "due_at"}))
      fail("NamedMilitaryCustodyTermShapeMismatch", term.id);
    const auto source = take(read_original_named_military_capture(
        context, term.capture_source, term.actor_id));
    if (term.recorded_tick > context.common.tick())
      require_actual_field_closing(context, source, term.recorded_tick);
    if (source.transfers_existing_custody ||
        source.receipt->tick != term.capture_tick ||
        term.captured_army_id != source.captured_army->id ||
        term.captured_organization_id !=
            source.captured_army->organization_id ||
        !same(term.army_formation_source, source.army_formation_source) ||
        term.army_formation_schema != source.army_formation_schema)
      fail("NamedMilitaryCustodyInitialCaptureRequired", term.id);
    if (!source.applied_at ||
        !same(term.capture_application_source, source.application_source) ||
        term.capture_actor_ordinal != source.actor_ordinal ||
        term.capture_application_schema != source.application_schema ||
        term.adopted_at.half_day_tick != term.capture_tick ||
        term.due_at.half_day_tick != term.due_tick ||
        take(compare_world_moments(term.adopted_at, *source.applied_at)) != 0)
      fail("NamedMilitaryCustodyActualApplicationRequired", term.id);
    const auto due = take(add_world_duration(
        term.adopted_at, half_days(term.duration_half_days)));
    if (previous_term && take(compare_world_moments(previous_term->adopted_at,
                                                    term.adopted_at)) > 0)
      fail("NamedMilitaryCustodyApplicationOrderMismatch", term.id);
    if (take(compare_world_moments(term.due_at, due)) != 0)
      fail("NamedMilitaryCustodyDeadlineMismatch", term.id);
    const auto at =
        position(context, {"military", "military_named_custody_term", term.id});
    if (position(context, term.capture_source) >= at ||
        (previous_sequence && previous_position >= at))
      fail("NamedMilitaryCustodyTermOrderMismatch", term.id);
    if (const auto prior = previous_terms.find(term.actor_id);
        prior != previous_terms.end())
      if (!closed_before(context, *prior->second,
                         position(context, term.capture_source)))
        fail("NamedMilitaryCustodyOverlappingTerms", term.actor_id);
    previous_terms[term.actor_id] = &term;
    previous_position = at;
    previous_sequence = serial;
    previous_tick = term.recorded_tick;
    previous_term = &term;
  }
  for (const auto &[id, head] : context.common.state().facts.rows())
    if (head.kind == "military_named_custody_term" &&
        (head.domain != "military" || !ids.contains(id)))
      fail("NamedMilitaryCustodyTermPayloadRequired", id);
  if (!selected)
    return;
  for (const auto &receipt : context.military.casualty_facts)
    for (const auto &change : receipt.actor_changes)
      if (change.status == "captured") {
        const SourceRef capture{"military", "military_casualties", receipt.id};
        const auto source = take(read_original_named_military_capture(
            context, capture, change.actor_id));
        const auto *term =
            term_for_actor_at_capture(context, change.actor_id, capture);
        if (source.transfers_existing_custody) {
          if (!term ||
              position(context, term->capture_source) >=
                  position(context, capture) ||
              position(context, {"military", "military_named_custody_term",
                                 term->id}) >= position(context, capture))
            fail("NamedMilitaryCustodyOriginalTermRequired", change.actor_id);
        } else if (!term || !same(term->capture_source, capture)) {
          if (&receipt != pending)
            fail("NamedMilitaryCustodyTermMissing", change.actor_id);
          if (term)
            fail("NamedMilitaryCustodyClosureSourceRequired", change.actor_id);
        }
      }
}
} // namespace
Outcome<bool> selected_named_military_custody(const CatalogIdentity &identity) {
  bool selected = false;
  for (const auto &[name, version] : identity.module_versions)
    if (name == "named_military_custody") {
      if (selected || version != "1")
        return {false,
                {},
                common_failure("UnsupportedNamedMilitaryCustodyVersion", name)
                    .error};
      selected = true;
    }
  return {true, selected, {}};
}
Status validate_named_military_custody_policy(
    const NamedMilitaryCustodyContext &context) {
  auto selected = selected_named_military_custody(context.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  const auto &policy = context.catalog.named_custody_policy;
  if (selected.value != bool(policy))
    return common_failure("NamedMilitaryCustodyPolicySelectionMismatch");
  if (policy && (policy->rule_id.empty() || policy->duration_half_days <= 0))
    return common_failure("NamedMilitaryCustodyPolicyRequired");
  if (!selected.value && (!context.military.named_custody.terms.empty() ||
                          !context.military.named_custody.closures.empty()))
    return common_failure("UnexpectedNamedMilitaryCustodyHistory");
  return common_success();
}
Status validate_named_military_custody_terms(
    const NamedMilitaryCustodyContext &context) {
  try {
    validate_rows(context);
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<const NamedMilitaryCustodyTerm *>
read_original_named_military_custody_term(
    const NamedMilitaryCustodyContext &context, const SourceRef &source) {
  try {
    validate_rows(context);
    if (source.domain != "military" ||
        source.kind != "military_named_custody_term")
      fail("NamedMilitaryCustodyTermSourceRequired", source.id);
    for (const auto &term : context.military.named_custody.terms)
      if (term.id == source.id)
        return {true, &term, {}};
    fail("NamedMilitaryCustodyTermSourceRequired", source.id);
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<std::vector<StableId>> record_named_military_custody_terms_in_candidate(
    CommonStore &common, MilitaryState &military,
    const NamedMilitaryCustodyContext &context,
    const MilitaryCasualtyFact &receipt) {
  try {
    if (&common != &context.common || &military != &context.military)
      fail("CrossCandidateNamedMilitaryCustody");
    const MilitaryCasualtyFact *actual = nullptr;
    for (const auto &row : military.casualty_facts)
      if (row.id == receipt.id) {
        if (actual)
          fail("NamedMilitaryCustodyDuplicateCapture", receipt.id);
        actual = &row;
      }
    if (actual != &receipt)
      fail("NamedMilitaryCustodyActualCaptureRequired", receipt.id);
    if (receipt.tick != common.tick())
      require_actual_field_receipt_closing(context, receipt);
    validate_rows(context, &receipt);
    if (!take(selected_named_military_custody(context.identity)))
      return {true, {}, {}};
    const auto &policy = *context.catalog.named_custody_policy;
    if (policy.duration_half_days >
        std::numeric_limits<Integer>::max() - receipt.tick)
      fail("NamedMilitaryCustodyDeadlineOverflow", receipt.id);
    std::vector<NamedMilitaryCustodyTerm> pending;
    std::vector<StableId> out;
    for (const auto &change : receipt.actor_changes)
      if (change.status == "captured") {
        const SourceRef source{"military", "military_casualties", receipt.id};
        const auto original = take(read_original_named_military_capture(
            context, source, change.actor_id));
        if (const auto *term =
                term_for_actor_at_capture(context, change.actor_id, source)) {
          if (!original.transfers_existing_custody &&
              !same(term->capture_source, source))
            fail("NamedMilitaryCustodyClosureSourceRequired", change.actor_id);
          out.push_back(term->id);
          continue;
        }
        if (original.transfers_existing_custody)
          fail("NamedMilitaryCustodyOriginalTermRequired", change.actor_id);
        if (!original.applied_at || !context.clock)
          fail("NamedMilitaryCustodyActualApplicationRequired", receipt.id);
        const auto now = take(context.clock->read_current_moment());
        if (take(compare_world_moments(*original.applied_at, now)) != 0)
          fail("NamedMilitaryCustodyActualApplicationRequired", receipt.id);
        NamedMilitaryCustodyTerm term;
        term.adopted_at = *original.applied_at;
        term.due_at = take(add_world_duration(
            term.adopted_at, half_days(policy.duration_half_days)));
        term.actor_id = change.actor_id;
        term.capture_source = source;
        term.capture_application_source = original.application_source;
        term.capture_actor_ordinal = original.actor_ordinal;
        term.capture_application_schema = original.application_schema;
        term.captured_army_id = original.captured_army->id;
        term.captured_organization_id = original.captured_army->organization_id;
        term.army_formation_source = original.army_formation_source;
        term.army_formation_schema = original.army_formation_schema;
        term.policy_source = policy_source(policy);
        term.capture_tick = term.recorded_tick = receipt.tick;
        term.duration_half_days = policy.duration_half_days;
        term.due_tick = receipt.tick + policy.duration_half_days;
        pending.push_back(std::move(term));
      }
    for (auto &term : pending) {
      term.id = take(common.next_id("named_military_custody"));
      take(common.append_fact(
          {term.id, "military_named_custody_term", "military"}));
      out.push_back(term.id);
      military.named_custody.terms.push_back(std::move(term));
    }
    validate_rows(context);
    // 原名册已落唯一占用，期限头必须先于 S4 唯一 availability 写入，不能在这里
    // 要求 available 已经变成 false。随后 S4/S1 真采用及完整 current validator
    // 缺一不可；任何失败由外层丢弃整 World，不得发布这个中间候选。
    for (const auto &change : receipt.actor_changes)
      if (change.status == "captured") {
        Integer held = 0;
        for (const auto &army : military.armies)
          for (const auto &actor : army.prisoner_ids)
            if (actor == change.actor_id) {
              if (army.id != change.captor_army_id)
                fail("NamedMilitaryCustodyOutcomeNotApplied", change.actor_id);
              ++held;
            }
        for (const auto &garrison : military.garrisons)
          if (std::find(garrison.prisoner_ids.begin(),
                        garrison.prisoner_ids.end(),
                        change.actor_id) != garrison.prisoner_ids.end())
            fail("NamedMilitaryCustodyOutcomeNotApplied", change.actor_id);
        if (held != 1)
          fail("NamedMilitaryCustodyOutcomeNotApplied", change.actor_id);
      }
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
