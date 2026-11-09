#include "military_result_projection2.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "exact_complete_world.hpp"
#include "common_fact_prefix.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_world_scope.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/military/military_exact_validation.hpp"
#include "resident/domains/military/military_authority_time.hpp"
#include "resident/domains/military/military_supply_exact.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/military/field_battle/field_binding_source.hpp"
#include <algorithm>
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/field_battle/field_state_codec.hpp"
#include "resident/domains/military/field_battle/field_contact_origin.hpp"
#include "resident/domains/military/field_battle/field_contact_moment.hpp"
#include "resident/domains/military/field_battle/field_boundary_source.hpp"
#include "resident/domains/military/field_battle/field_boundary_install.hpp"
#include "resident/domains/travel/movement_exact_model.hpp"
#include "resident/domains/military/field_battle/field_started_source.hpp"
#include "resident/domains/military/field_battle/field_reinforcement_queue.hpp"
#include "resident/domains/military/field_battle/field_commander_change_source.hpp"
#include "resident/domains/military/field_battle/field_stratagem_resolution.hpp"
#include "resident/domains/military/field_battle/field_council_validation.hpp"
#include "resident/domains/military/field_battle/field_settlement_exit_source.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ready.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ordinary_loss.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
enum class SupportingResultOwner {
  none,
  dynamic,
  duty,
  training,
  naval_repair
};
SupportingResultOwner owner(const SourceRef &s) {
  if (s.domain == "military_naval") {
    for (const auto *kind :
         {"naval_repair_start", "naval_repair_result", "naval_repair_charge"})
      if (s.kind == kind)
        return SupportingResultOwner::naval_repair;
    return SupportingResultOwner::none;
  }
  if (s.domain != "military")
    return SupportingResultOwner::none;
  for (const auto *kind :
       {"military_training", "military_training_completed",
        "military_training_lesson", "military_training_participation",
        "military_training_period_witness", "military_training_drill_frozen",
        "military_training_cancelled"})
    if (s.kind == kind)
      return SupportingResultOwner::training;
  for (const auto *kind :
       {"dynamic_session_state", "dynamic_tactical_action",
        "dynamic_tactical_recovery", "dynamic_effect_group", "dynamic_effect",
        "military_battle", "military_casualties", "military_objective_result"})
    if (s.kind == kind)
      return SupportingResultOwner::dynamic;
  for (const auto *kind :
       {"military_duty_terms", "military_duty_window", "military_duty_period",
        "military_duty_control", "military_station_source",
        "military_station_terminal", "military_duty_execution_bound",
        "military_duty_execution_stopped"})
    if (s.kind == kind)
      return SupportingResultOwner::duty;
  return SupportingResultOwner::none;
}
} // namespace
bool native_military_supporting_source2(const SourceRef &s) {
  return owner(s) != SupportingResultOwner::none;
}
Outcome<WorldMoment>
NativeMilitarySupportingResultRead2::read_selected_source_moment(
    const MomentOwnerSource &source) const {
  switch (owner(source.source_ref)) {
  case SupportingResultOwner::dynamic:
    return read_verified_dynamic_military_source_moment(
        x_.common, x_.military, x_.exact, x_.dynamic, source);
  case SupportingResultOwner::duty:
    return read_verified_military_duty_source_moment(x_.common, x_.duty,
                                                     x_.duty_original, source);
  case SupportingResultOwner::training:
    if (!x_.training)
      return {false,
              {},
              common_failure("MilitaryTrainingOriginalResultReadRequired",
                             source.source_ref.id,
                             ErrorKind::unsupported_capability)
                  .error};
    return read_verified_exact_military_training_source_moment(
        x_.common, x_.military, x_.training->state, x_.training->elapsed,
        x_.training->original, source.source_ref, source.source_schema);
  case SupportingResultOwner::naval_repair: {
    if (!x_.naval_repair)
      return {false,
              {},
              common_failure("NavalRepairOriginalResultReadRequired",
                             source.source_ref.id,
                             ErrorKind::unsupported_capability)
                  .error};
    const auto &access = *x_.naval_repair;
    auto original = read_verified_exact_naval_repair_source(
        x_.common, x_.military, access.naval, access.state, access.elapsed,
        access.original, source.source_ref, source.source_schema);
    if (!original.ok)
      return {false, {}, original.error};
    return {true, std::move(original.value.occurred_at), {}};
  }
  default:
    return {false,
            {},
            common_failure("UnsupportedMilitarySupportingSource",
                           source.source_ref.id,
                           ErrorKind::unsupported_capability)
                .error};
  }
}
Outcome<ExactOwnerFactRead>
NativeMilitarySupportingResultRead2::read_selected_fact(
    const FactRecord &h) const {
  const auto *actual = x_.common.state().facts.find(h.id);
  if (!actual || actual->id != h.id || actual->kind != h.kind ||
      actual->domain != h.domain)
    return {false,
            {},
            common_failure("MilitarySupportingHeaderMismatch", h.id).error};
  const SourceRef ref{h.domain, h.kind, h.id};
  Outcome<Integer> schema;
  switch (owner(ref)) {
  case SupportingResultOwner::dynamic:
    schema = read_dynamic_military_source_schema(ref);
    break;
  case SupportingResultOwner::duty:
    schema = read_military_duty_source_schema(ref);
    break;
  case SupportingResultOwner::training:
    if (ref.kind == "military_training_completed")
      return {false,
              {},
              common_failure("MilitaryTrainingCompletedIsOwnedField", h.id,
                             ErrorKind::internal)
                  .error};
    schema = {true, 2, {}};
    break;
  case SupportingResultOwner::naval_repair:
    if (ref.kind == "naval_repair_charge")
      return {false,
              {},
              common_failure("NavalRepairChargeIsOwnedField", h.id,
                             ErrorKind::internal)
                  .error};
    schema = {true, 2, {}};
    break;
  default:
    return {false,
            {},
            common_failure("UnsupportedMilitarySupportingSource", h.id,
                           ErrorKind::unsupported_capability)
                .error};
  }
  if (!schema.ok)
    return {false, {}, schema.error};
  MomentOwnerSource source{ref, schema.value};
  auto at = read_selected_source_moment(source);
  if (!at.ok)
    return {false, {}, at.error};
  ExactOwnerFactRead out;
  out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
  out.supporting_sources.push_back(std::move(source));
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
struct FieldResultFailure { RuleError error; };
template <class T> T field_result_take(Outcome<T> value) {
  if (!value.ok) throw FieldResultFailure{std::move(value.error)};
  return std::move(value.value);
}
void field_result_take(Status value) {
  if (!value.ok) throw FieldResultFailure{std::move(value.error)};
}
[[noreturn]] void field_result_fail(const char *code, const StableId &id = {}) {
  throw FieldResultFailure{common_failure(code, id, ErrorKind::unsupported_capability).error};
}
template <class T, class F> Outcome<T> field_result_guard(F &&f) {
  try { return {true, f(), {}}; }
  catch (FieldResultFailure &e) { return {false, {}, std::move(e.error)}; }
  catch (const std::bad_alloc &) {
    return {false, {}, common_failure("FieldMilitaryResultResourceExhausted", {}, ErrorKind::internal).error};
  }
  catch (const std::length_error &) {
    return {false, {}, common_failure("FieldMilitaryResultResourceExhausted", {}, ErrorKind::internal).error};
  }
}
bool field_result_is(const StableId &kind, std::initializer_list<const char *> values) {
  for (const auto *value : values) if (kind == value) return true;
  return false;
}
// 临时来源引用保真实 job 资源，普通复制；只给原读口借身份，不创建新来源行。
struct FieldResultTemporarySource {
  CheckpointMemoryCharge memory;
  SourceRef ref;
  FieldResultTemporarySource(std::string_view kind, const StableId &id) {
    memory = CheckpointMemoryCharge(0);
    ref = {"military", std::string(kind), id};
  }
};
Integer field_result_schema(const StableId &kind) {
  return field_result_is(kind, {"military_action", "military_action_completed",
      "military_office_completion_applied", "military_action_completion_applied",
      "military_formation_completion_checked"}) ? 2 : 1;
}
template <class Rows> const typename Rows::value_type *field_result_unique(
    const Rows &rows, const StableId &id) {
  const typename Rows::value_type *found = nullptr;
  for (const auto &row : rows) if (row.id == id) {
    if (found) field_result_fail("FieldMilitaryResultDuplicateOwner", id);
    found = &row;
  }
  if (!found) field_result_fail("FieldMilitaryResultOriginalRowRequired", id);
  return found;
}
const NativeCompleteWorldCoreSources &field_result_core(
    const WorldBundle &world, const CompleteExactWorldBindingOwner &owner,
    const NativeCompleteWorldCoreSources &core) {
  if (&core.characters.early.world != &world ||
      &core.characters.early.complete != &owner)
    field_result_fail("FieldMilitaryResultSameWorldOwnerRequired");
  return core;
}
// 原 Field 接敌验证的有限来源部分：同一实际接触、原两军绑定、当刻主将、
// 原开程具名成员与敌对关系。几何重建不能代替这些真实记录关联。
WorldMoment field_result_initial_contact(const WorldBundle &world,
    const NativeFieldWorldScope &scope, const SourceRef &source,
    ExactArithmeticBudget budget) {
  const auto &x = *scope.exact_contacts();
  const auto &catalog = *world.catalog->field_battle;
  const auto original = field_result_take(read_field_initial_contact_actual_moment(
      x.common, x.military, x.field, catalog, source, x, budget));
  const auto &record = *original.field_contact;
  const auto *origin = std::get_if<FieldExactGroupContactOrigin>(&record.source);
  if (!origin || !scope.diplomacy())
    field_result_fail("FieldMilitaryResultInitialContactOwnerRequired", source.id);
  const auto actual = field_result_take(read_field_exact_road_contact_source(
      x, origin->actual_contact, catalog.parameters, budget));
  const auto relation = field_result_take(read_political_relation_view_before_field_source(
      *scope.diplomacy(), source, *actual.endpoints[0].faction_id, *actual.endpoints[1].faction_id));
  if (relation.relation != PoliticalRelation::hostile)
    field_result_fail("FieldMilitaryResultHostileContactRequired", source.id);
  for (const auto &endpoint : actual.endpoints) {
    const auto index = endpoint.geometry_endpoint == &actual.contact->original_hit.road->first ? 0 : 1;
    const auto &binding = record.original_bindings[index];
    if (field_result_take(read_field_original_battle_binding(x.common, x.field,
            record.battle_id, binding.army_id)) != &binding ||
        binding.army_id != endpoint.army->id || !binding.action_id.empty())
      field_result_fail("FieldMilitaryResultOriginalBindingRequired", source.id);
    const auto command = field_result_take(query_military_army_command_source_before_at(
        x.common, x.military, endpoint.army->id, origin->actual_contact.id,
        actual.contact->occurred_at, &x.formations));
    if (binding.commander_actor_id != command.commander_actor_id ||
        binding.members.size() != endpoint.opening->named_participants.size())
      field_result_fail("FieldMilitaryResultOriginalBindingRequired", source.id);
    for (const auto &name : endpoint.opening->named_participants)
      if (std::count(endpoint.opening->named_participants.begin(), endpoint.opening->named_participants.end(), name) != 1 ||
          std::count_if(binding.members.begin(), binding.members.end(),
              [&](const auto &member) { return member.actor_id == name; }) != 1)
        field_result_fail("FieldMilitaryResultOriginalBindingRequired", source.id);
  }
  return *original.occurred_at;
}
// 只投影显式 Field 原来源；不回调 Complete 采集、整 World 校验或业务写者。
class NativeFieldMilitaryResultRead final : public ExactResultOwnerRead,
                                           public CheckpointAllocatedObject {
  const WorldBundle &world_;
  const CompleteExactWorldBindingOwner &owner_;
  const NativeCompleteWorldCoreSources &core_;

  std::optional<WorldMoment> read_original(const MomentOwnerSource &source) const {
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "P10.field_military.original", &world_.common.state(), nullptr, {"fact", source.source_ref.domain, source.source_ref.kind, source.source_ref.id, source.source_schema}, {}, -1});
    const auto &ref = source.source_ref;
    if (!native_field_military_result_kind(ref.domain, ref.kind) || ref.id.empty())
      field_result_fail("UnsupportedFieldMilitaryResultSource", ref.id);
    if (source.source_schema != field_result_schema(ref.kind))
      field_result_fail("FieldMilitaryResultSourceSchemaRequired", ref.id);
    const auto &core = field_result_core(world_, owner_, core_);
    const auto now = field_result_take(world_.actual_phase_moment());
    const auto same_now = field_result_take(core.characters.early.clock.read_current_moment());
    if (field_result_take(compare_world_moments(now, same_now)) != 0)
      field_result_fail("FieldMilitaryResultActualClockRequired", ref.id);
    const auto selected = field_result_take(read_military_exact_phase_selection(*world_.catalog));
    if (!selected.field_original)
      field_result_fail("FieldMilitaryResultSelectionRequired", ref.id);
    auto lease = field_result_take(owner_.read_military(now));
    if (!lease) field_result_fail("FieldMilitaryResultOriginalLeaseRequired", ref.id);
    const auto x = lease->bindings();
    if (&x.common != &world_.common || &x.military != &world_.military ||
        !world_.catalog->military || &x.catalog != world_.catalog->military.get() ||
        x.access.common_catalog != world_.catalog->common.get() ||
        x.access.interaction != &world_.interaction ||
        !world_.action_elapsed || &x.elapsed != &*world_.action_elapsed ||
        !world_.military_action_time || x.action_time != &*world_.military_action_time ||
        field_result_take(compare_world_moments(now, field_result_take(x.clock.read_current_moment()))) != 0)
      field_result_fail("FieldMilitaryResultSameWorldOwnerRequired", ref.id);
    // completed 是原行动的拥有字段，不是另一个 Common 事实头。
    const auto *head = world_.common.state().facts.find(ref.id);
    const auto expected = ref.kind == "military_action_completed" ? "military_action" : ref.kind.c_str();
    if (!head || head->id != ref.id || head->domain != "military" || head->kind != expected)
      field_result_fail("FieldMilitaryResultHeaderMismatch", ref.id);

    std::optional<WorldMoment> result;
    if (ref.kind == "military_supply") {
      if (!x.field_supply || !x.field_supply->field_supply_source1)
        field_result_fail("FieldMilitaryResultSupplyOwnerRequired", ref.id);
      const auto original = field_result_take(read_field_military_supply_source(
          x.common, x.military, x.field_supply->periods, ref, 1, now, x.budget));
      result = original.original_period.opening_at;
    } else if (ref.kind == "campaign_accepted" || ref.kind == "office_resignation") {
      if (x.access.military_authority_time_schema != 1 || !x.access.authority_time)
        field_result_fail("FieldMilitaryResultAuthorityOwnerRequired", ref.id);
      if (ref.kind == "campaign_accepted") {
        const auto *row = field_result_take(read_military_campaign_acceptance_source(
            *x.access.authority_time, ref, 1));
        if (!row->exact_acceptance) field_result_fail("FieldMilitaryResultAuthorityOwnerRequired", ref.id);
        result = row->exact_acceptance->accepted_at;
      } else {
        const auto *row = field_result_take(read_military_office_resignation_source(
            *x.access.authority_time, ref, 1));
        if (!row->exact_resignation) field_result_fail("FieldMilitaryResultAuthorityOwnerRequired", ref.id);
        result = row->exact_resignation->resigned_at;
      }
    } else if (field_result_schema(ref.kind) == 2) {
      if (ref.kind == "military_office_completion_applied") {
        if (!selected.actions.office_accept || !x.office)
          field_result_fail("FieldMilitaryResultOfficeOwnerRequired", ref.id);
        const auto *row = field_result_take(read_verified_exact_office_application(
            x.common, x.military, *x.action_time, x.elapsed, *x.office, ref, 2));
        result = *row->completed_at;
      } else if (ref.kind == "military_action_completion_applied" ||
                 ref.kind == "military_formation_completion_checked") {
        if (!selected.actions.army_form || !x.formation)
          field_result_fail("FieldMilitaryResultFormationOwnerRequired", ref.id);
        const auto *row = ref.kind == "military_action_completion_applied"
            ? field_result_take(read_verified_exact_formation_application(
                x.common, x.military, *x.action_time, x.elapsed, *x.formation, ref, 2))
            : field_result_take(read_exact_formation_completion_check(
                x.common, x.military, *x.action_time, x.elapsed, x.formation->scale,
                x.clock, ref, 2, x.budget));
        result = *row->completed_at;
      } else {
        const auto *row = x.action_time->dynamic_action_facts.find(ref.id);
        if (!row) field_result_fail("FieldMilitaryResultOriginalActionRequired", ref.id);
        if (row->command == "office_accept" && selected.actions.office_accept && x.office) {
          NativeOfficeActionTimeRead original(x.common, x.military, *x.action_time, x.elapsed, *x.office);
          field_result_take(original.read_action_time_owner(exact_office_action_registration(), ref.id));
          if (ref.kind == "military_action_completed")
            field_result_take(read_verified_exact_office_result(x.common, x.military,
                *x.action_time, x.elapsed, *x.office, ref, 2));
          result = field_result_take(original.read_action_time_source(source));
        } else if (row->command == "army_form" && selected.actions.army_form && x.formation) {
          NativeFormationActionTimeRead original(x.common, x.military, *x.action_time, x.elapsed, *x.formation);
          field_result_take(original.read_action_time_owner(exact_formation_action_registration(), ref.id));
          if (ref.kind == "military_action_completed")
            field_result_take(read_verified_exact_formation_result(x.common, x.military,
                *x.action_time, x.elapsed, *x.formation, ref, 2));
          result = field_result_take(original.read_action_time_source(source));
        } else field_result_fail("FieldMilitaryResultOriginalActionRequired", ref.id);
      }
    } else if (ref.kind == "campaign_arrival") {
      const auto *row = field_result_unique(x.military.arrival_facts, ref.id);
      const auto *campaign = field_result_unique(x.military.campaigns, row->campaign_id);
      if (!campaign->exact_acceptance || !x.access.authority_time ||
          field_result_take(read_military_campaign_acceptance_source(*x.access.authority_time,
              campaign->exact_acceptance->actual_acceptance_source, 1)) != campaign ||
          field_result_take(read_actual_common_fact_position(x.common,
              campaign->exact_acceptance->actual_acceptance_source)) >=
              field_result_take(read_actual_common_fact_position(x.common, ref)))
        field_result_fail("FieldMilitaryResultCalendarOriginalAcceptanceRequired", ref.id);
      if (row->army_id != campaign->army_id || row->actor_id != campaign->actor_id ||
          campaign->arrival_fact_id != row->id || row->tick < campaign->accepted_tick ||
          row->tick > now.half_day_tick || row->stronghold_id.empty())
        field_result_fail("FieldMilitaryResultCalendarSourceMismatch", ref.id);
      const auto *order = field_result_unique(x.catalog.orders, campaign->template_id);
      if (order->target_stronghold_id != row->stronghold_id)
        field_result_fail("FieldMilitaryResultCalendarTargetMismatch", ref.id);
      field_result_unique(x.catalog.strongholds, row->stronghold_id);
      // 原整数日历回执保原精度，绝不伪造精确发生时刻。
    } else if (ref.kind == "campaign_settlement") {
      const auto *row = field_result_unique(x.military.settlement_facts, ref.id);
      const auto *campaign = field_result_unique(x.military.campaigns, row->campaign_id);
      if (!campaign->exact_acceptance || !x.access.authority_time ||
          field_result_take(read_military_campaign_acceptance_source(*x.access.authority_time,
              campaign->exact_acceptance->actual_acceptance_source, 1)) != campaign ||
          field_result_take(read_actual_common_fact_position(x.common,
              campaign->exact_acceptance->actual_acceptance_source)) >=
              field_result_take(read_actual_common_fact_position(x.common, ref)))
        field_result_fail("FieldMilitaryResultCalendarOriginalAcceptanceRequired", ref.id);
      if (row->army_id != campaign->army_id || row->actor_id != campaign->actor_id ||
          campaign->settlement_id != row->id || row->tick < campaign->accepted_tick ||
          row->tick > now.half_day_tick || row->reason.empty())
        field_result_fail("FieldMilitaryResultCalendarSourceMismatch", ref.id);
    } else {
      // 以下借用先于真实军事租约析构；直接借已构成 core，不回调 Complete 构造 API。
      auto field = field_result_take(NativeFieldWorldScope::create_from_original_core(world_, core));
      if (!field || !field->bound_to(world_) || !field->exact_contacts())
        field_result_fail("FieldMilitaryResultFieldOwnerRequired", ref.id);
      result = read_field(ref, x, *field);
    }
    if (result && field_result_take(compare_world_moments(*result, now)) > 0)
      field_result_fail("FieldMilitaryResultFutureSource", ref.id);
    return result;
  }

  std::optional<WorldMoment> read_field(const SourceRef &ref,
      const MilitaryExactReadBinding &x, const NativeFieldWorldScope &scope) const;
public:
  NativeFieldMilitaryResultRead(const WorldBundle &world,
      const CompleteExactWorldBindingOwner &owner, const NativeCompleteWorldCoreSources &core)
      : world_(world), owner_(owner), core_(core) {}
  Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource &source) const override {
    return field_result_guard<WorldMoment>([&] {
      const auto at = read_original(source);
      world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "P10.field_military.original_validated", &world_.common.state(), nullptr, {"fact", source.source_ref.domain, source.source_ref.kind, source.source_ref.id, source.source_schema}, {}, -1});
      if (!at) field_result_fail("FieldMilitaryResultExactMomentUnsupported", source.source_ref.id);
      return *at;
    });
  }
  Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord &h) const override {
    return field_result_guard<ExactOwnerFactRead>([&] {
      const auto *actual = world_.common.state().facts.find(h.id);
      if (!actual || actual->id != h.id || actual->kind != h.kind || actual->domain != h.domain)
        field_result_fail("FieldMilitaryResultHeaderMismatch", h.id);
      const auto memory = CheckpointMemoryCharge(0);
      const MomentOwnerSource source{{h.domain, h.kind, h.id}, field_result_schema(h.kind)};
      const auto at = read_original(source);
      world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "P10.field_military.original_validated", &world_.common.state(), nullptr, {"fact", source.source_ref.domain, source.source_ref.kind, source.source_ref.id, source.source_schema}, {}, -1});
      ExactOwnerFactRead out;
      out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
      if (at) {
        out.payload_memory = CheckpointCopyLease::reserve();
        out.supporting_sources.reserve(1);
        out.supporting_sources.push_back(source);
      }
      return out;
    });
  }
};

std::optional<WorldMoment> NativeFieldMilitaryResultRead::read_field(
    const SourceRef &ref, const MilitaryExactReadBinding &x,
    const NativeFieldWorldScope &scope) const {
  const auto &field = *world_.field_battle;
  const auto &catalog = *world_.catalog->field_battle;
  const auto &contacts = *scope.exact_contacts();
  const auto &abilities = scope.abilities();
  field_result_take(validate_field_battle_fact_headers(world_.common, field));
  if (!contacts.clock.bound_to(world_.common, world_.military, field) ||
      &contacts.common != &world_.common || &contacts.military != &world_.military ||
      &contacts.field != &field || abilities.exact_contacts != &contacts)
    field_result_fail("FieldMilitaryResultSameWorldOwnerRequired", ref.id);
  if (ref.kind == "field_battle_contact") {
    const auto initial = field.contacts.find(ref.id);
    if (initial != field.contacts.end()) {
      return field_result_initial_contact(world_, scope, ref, x.budget);
    }
    const auto boundary = field.boundary_contacts.find(ref.id);
    if (boundary == field.boundary_contacts.end())
      field_result_fail("FieldMilitaryResultBoundaryOwnerRequired", ref.id);
    const auto *origin = std::get_if<FieldExactBoundaryOrigin>(&boundary->second.motion);
    if (!origin) field_result_fail("FieldMilitaryResultExactBoundaryRequired", ref.id);
    if (!scope.diplomacy()) field_result_fail("FieldMilitaryResultDiplomacyRequired", ref.id);
    field_result_take(validate_field_exact_boundary_contacts(contacts, *scope.diplomacy(), catalog, x.budget));
    const auto original = field_result_take(read_field_boundary_original_source(
        contacts, catalog, origin->actual_contact, x.budget));
    return original.contact->occurred_at;
  }
  if (ref.kind == "field_battle_session" || ref.kind == "field_battle_started") {
    const FieldBattleSession *session = nullptr;
    for (const auto &[id, row] : field.sessions)
      if ((ref.kind == "field_battle_session" && id == ref.id) ||
          (ref.kind == "field_battle_started" && row.started_source && row.started_source->id == ref.id)) {
        if (session) field_result_fail("FieldMilitaryResultDuplicateOwner", ref.id);
        session = &row;
      }
    if (!session) field_result_fail("FieldMilitaryResultSessionRequired", ref.id);
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, row] : field.contacts) if (row.battle_id == session->battle_id) {
      if (contact) field_result_fail("FieldMilitaryResultDuplicateOwner", ref.id);
      contact = &row;
    }
    if (!contact) field_result_fail("FieldMilitaryResultSessionContactRequired", ref.id);
    field_result_take(validate_field_contact_origin_head(x.common, *contact));
    const FieldResultTemporarySource contact_source("field_battle_contact", contact->contact_id);
    field_result_initial_contact(world_, scope, contact_source.ref, x.budget);
    const auto generated = field_result_take(regenerate_field_contact_battle(
        contacts.travel, catalog, contact->source, &contacts, x.budget));
    if (generated.field_hash != session->field_hash)
      field_result_fail("FieldMilitaryResultSessionFieldMismatch", ref.id);
    if (session->started_source && session->started_source->id == ref.id) {
      const auto original = field_result_take(read_field_battle_started_source(
          x.common, x.military, field, ref, contacts.clock, contacts.formations, x.budget));
      return *original.started_actual_at;
    }
    if (ref.kind == "field_battle_started")
      field_result_fail("FieldMilitaryResultStartedSourceRequired", ref.id);
    // 未启动军议会话只有原 created_tick/局部时刻，不另造精确 M。
    return {};
  }
  if (ref.kind == "field_battle_entry_queue" || ref.kind == "field_battle_entered") {
    if (!abilities.original_rosters)
      field_result_fail("FieldMilitaryResultOriginalRosterRequired", ref.id);
    const FieldBattleBoundaryRecord *boundary = nullptr;
    const FieldReinforcementAdmission *admission = nullptr;
    for (const auto &[id, row] : field.boundary_contacts) {
      bool owns = false;
      if (row.reinforcement_queue) {
        const auto &queue = *row.reinforcement_queue;
        owns = ref.kind == "field_battle_entry_queue" && queue.initialized_source.id == ref.id;
        if (ref.kind == "field_battle_entered") for (const auto &entry : queue.admitted)
          if (entry.application_source.id == ref.id) {
            if (admission) field_result_fail("FieldMilitaryResultDuplicateOwner", ref.id);
            admission = &entry; owns = true;
          }
      } else if (ref.kind == "field_battle_entered" && row.entry_application_source &&
                 row.entry_application_source->id == ref.id) owns = true;
      if (owns) {
        if (boundary) field_result_fail("FieldMilitaryResultDuplicateOwner", ref.id);
        boundary = &row;
      }
    }
    if (!boundary) field_result_fail("FieldMilitaryResultEntryOwnerRequired", ref.id);
    if (!scope.diplomacy()) field_result_fail("FieldMilitaryResultDiplomacyRequired", ref.id);
    field_result_take(validate_field_exact_boundary_contacts(contacts, *scope.diplomacy(), catalog, x.budget));
    if (boundary->reinforcement_queue) {
      const FieldResultTemporarySource contact_source("field_battle_contact", boundary->contact_id);
      const auto *original = field_result_take(read_field_original_reinforcement_queue(
          contacts, catalog, *abilities.original_rosters, contact_source.ref, x.budget));
      if (original != &*boundary->reinforcement_queue)
        field_result_fail("FieldMilitaryResultEntryOwnerRequired", ref.id);
      if (ref.kind == "field_battle_entry_queue") return original->initialized_at;
      if (!admission) field_result_fail("FieldMilitaryResultEntryOwnerRequired", ref.id);
      const auto session = field.sessions.find(boundary->battle_id);
      if (session == field.sessions.end()) field_result_fail("FieldMilitaryResultSessionRequired", ref.id);
      const auto formation = session->second.formations.find(admission->formation_id);
      if (formation == session->second.formations.end() ||
          field_result_take(read_field_reinforcement_admission_identity(x.common, field,
              session->second, formation->second)) != admission)
        field_result_fail("FieldMilitaryResultEntryOwnerRequired", ref.id);
      return admission->applied_at;
    }
    // 旧无队列入场仍须通过原边界/入场门，只读真实应用字段，不补造队列。
    if (!boundary->entered_actual_at)
      field_result_fail("FieldMilitaryResultEntryMomentRequired", ref.id);
    return *boundary->entered_actual_at;
  }
  if (ref.kind == "field_battle_segment")
    return field_result_take(read_field_segment_application_moment(x.common, x.military,
        field, ref, contacts.clock));
  if (ref.kind == "military_battle") {
    const auto *row = field_result_unique(x.military.battle_facts, ref.id);
    const FieldResultTemporarySource parent_source("field_battle_segment", row->source_id);
    const auto original = field_result_take(read_field_segment_settlement_source(x.common,
        x.military, field, parent_source.ref, &contacts.clock));
    if (original.terminal != row) field_result_fail("FieldMilitaryResultTerminalOwnerRequired", ref.id);
    return original.settlement->applied_at;
  }
  if (ref.kind == "military_casualties") {
    const auto *row = field_result_unique(x.military.casualty_facts, ref.id);
    const auto *parent = x.common.state().facts.find(row->battle_id);
    if (!parent || parent->domain != "military")
      field_result_fail("FieldMilitaryResultCasualtyParentRequired", ref.id);
    if (parent->kind == "field_battle_segment") {
      const FieldResultTemporarySource parent_source("field_battle_segment", row->battle_id);
      const auto original = field_result_take(read_field_segment_settlement_source(x.common,
          x.military, field, parent_source.ref, &contacts.clock));
      if (original.casualties != row) field_result_fail("FieldMilitaryResultCasualtyParentRequired", ref.id);
      return original.settlement->applied_at;
    }
    if (parent->kind != "field_army_exit_step")
      field_result_fail("FieldMilitaryResultCasualtyParentRequired", ref.id);
    const auto original = field_result_take(read_field_army_exit_ordinary_casualty_source(
        x.common, x.military, field, ref, contacts.clock));
    const FieldResultTemporarySource parent_source("field_army_exit_step", row->battle_id);
    const auto full = field_result_take(read_field_army_exit_step_source(contacts, catalog,
        abilities, parent_source.ref));
    if (original.casualties != row || full.step != original.step || full.plan != original.plan ||
        full.session != original.session)
      field_result_fail("FieldMilitaryResultCasualtyParentRequired", ref.id);
    return full.step->applied_at;
  }
  if (ref.kind == "field_commander_changed") {
    const auto original = field_result_take(read_field_commander_change_source(x.common, field, ref));
    return original.change->applied_at;
  }
  if (ref.kind == "field_battle_stratagem" || ref.kind == "military_council") {
    const FieldBattleSession *session = nullptr;
    const FieldStratagemRecord *stratagem = nullptr;
    const FieldCouncilFact *council = nullptr;
    if (ref.kind == "military_council") {
      const auto found = field.council_facts.find(ref.id);
      if (found == field.council_facts.end()) field_result_fail("FieldMilitaryResultCouncilRequired", ref.id);
      council = &found->second;
      const auto s = field.sessions.find(council->battle_id);
      if (s != field.sessions.end()) session = &s->second;
    } else for (const auto &[id, s] : field.sessions)
      for (const auto &[key, value] : s.stratagems)
        if (value.adopted_source.id == ref.id && value.kind == FieldStratagemKind::Confuse) {
          if (stratagem) field_result_fail("FieldMilitaryResultDuplicateOwner", ref.id);
          stratagem = &value; session = &s;
        }
    if (!session) field_result_fail("FieldMilitaryResultSessionRequired", ref.id);
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, row] : field.contacts) if (row.battle_id == session->battle_id) {
      if (contact) field_result_fail("FieldMilitaryResultDuplicateOwner", ref.id);
      contact = &row;
    }
    if (!contact) field_result_fail("FieldMilitaryResultSessionContactRequired", ref.id);
    field_result_take(validate_field_contact_origin_head(x.common, *contact));
    const FieldResultTemporarySource contact_source("field_battle_contact", contact->contact_id);
    field_result_initial_contact(world_, scope, contact_source.ref, x.budget);
    const auto generated = field_result_take(regenerate_field_contact_battle(
        contacts.travel, catalog, contact->source, &contacts, x.budget));
    if (generated.field_hash != session->field_hash)
      field_result_fail("FieldMilitaryResultSessionFieldMismatch", ref.id);
    if (stratagem) {
      field_result_take(validate_field_stratagem_sources(x.common, field, *session, generated, abilities));
      return stratagem->applied_at;
    }
    field_result_take(validate_field_council_records(x.common, *session, generated));
    field_result_take(validate_field_council_facts(x.common, field, abilities.original_life));
    return council->adopted_actual_at;
  }
  if (ref.kind == "field_battle_exit") {
    const auto *original = field_result_take(read_field_actual_exit_application(x.common,
        x.military, field, contacts.travel, catalog, ref, contacts.clock, x.budget,
        &contacts, scope.diplomacy(), abilities.territory));
    return original->applied_at;
  }
  if (ref.kind == "field_army_exit_choice") {
    const auto *original = field_result_take(read_field_army_exit_choice_source(
        contacts, catalog, abilities, x.access, ref));
    return original->applied_at;
  }
  if (ref.kind == "field_army_exit_step")
    return field_result_take(read_field_army_exit_step_source(contacts, catalog, abilities, ref)).step->applied_at;
  if (ref.kind == "field_army_exit_ready")
    return field_result_take(read_field_army_exit_ready_source(contacts, catalog, abilities, ref, x.budget)).adopted_at;
  field_result_fail("UnsupportedFieldMilitaryResultSource", ref.id);
}
} // namespace

bool native_field_military_result_kind(const StableId &domain, const StableId &kind) {
  return domain == "military" && (owns_field_battle_fact_kind(kind) ||
      field_result_is(kind, {"military_supply", "campaign_accepted", "office_resignation",
          "military_action", "military_action_completed", "military_office_completion_applied",
          "military_action_completion_applied", "military_formation_completion_checked",
          "campaign_arrival", "campaign_settlement", "military_battle", "military_casualties"}));
}
Outcome<std::unique_ptr<const ExactResultOwnerRead>> make_native_field_military_result_read(
    const WorldBundle &world, const CompleteExactWorldBindingOwner &owner,
    const NativeCompleteWorldCoreSources &core) {
  return field_result_guard<std::unique_ptr<const ExactResultOwnerRead>>([&]() -> std::unique_ptr<const ExactResultOwnerRead> {
    if (!world.catalog) field_result_fail("FieldMilitaryResultCatalogRequired");
    field_result_take(validate_field_battle_layout(world.catalog->identity,
        world.catalog->field_battle.get(), world.field_battle ? &*world.field_battle : nullptr));
    const auto selected = field_result_take(selected_field_battle_version(world.catalog->identity));
    if (!selected) return {};
    if (!world.catalog->exact_time || !exact_complete_selected(*world.catalog->exact_time) ||
        !world.action_elapsed || !world.military_action_time)
      field_result_fail("FieldMilitaryResultOriginalRootsRequired");
    const auto phases = field_result_take(read_military_exact_phase_selection(*world.catalog));
    if (!phases.field_original) field_result_fail("FieldMilitaryResultSelectionRequired");
    field_result_core(world, owner, core);
    return std::unique_ptr<const ExactResultOwnerRead>(new NativeFieldMilitaryResultRead(world, owner, core));
  });
}
} // namespace hanmo::resident
