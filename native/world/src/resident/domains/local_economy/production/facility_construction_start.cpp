// 把真实本人、资源与原 Action/Fixed 接成同一候选，不沿旧 EstateBuild 替代。
#include "facility_construction_start.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <stdexcept>

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
StableId fresh(const StableId &value) {
  return checkpoint_fresh_string_copy(value);
}
// 内部固定 kind 也按首次 range 构造预留，不能借 string 常量的长度猜旧容量。
StableId literal(std::string_view value) {
  StableId out(value.data(), value.size());


  return out;
}
// 同一个实际私有 World 的原根与同 job 字段 owner；跨根借额度始终拒。
void candidate(const FacilityConstructionStartWriteContext &x) {
  const auto memory = current_checkpoint_memory();
  const auto &owner = x.cost.owned_fields.resource();
  if (&x.service.adoption != &x.cost.read || &x.elapsed != &x.service.elapsed ||
      &x.cost.common != &x.cost.read.common ||
      &x.cost.state != &x.cost.read.state || !memory || !owner ||
      !memory->shares_job(*owner))
    fail("ConstructionActualStartCandidateRequired");
}
// 只在原 source 的同步借用范围内复制一次字段；原 lease 留在候选根中。
class WorkerCapture final : public FacilityConstructionWorkerAdoptionSink {
  CheckpointMemoryCharge &fields_;
  FacilityConstruction &row_;
  bool captured_ = false;

public:
  WorkerCapture(CheckpointMemoryCharge &fields, FacilityConstruction &row)
      : fields_(fields), row_(row) {}
  Status accept(const ActorConditionHead &head,
                const ActualSpatialSourceRef &presence,
                const WorldMoment &at) override {
    if (captured_)
      return common_failure("ConstructionDuplicateWorkerCapture");


    (fields_).grow(0);
    row_.started_at = at;
    row_.accepted_condition_head = {
        fresh(head.actor_id),
        head.condition_revision,
        {fresh(head.initial_condition_seed_ref.seed_key)},
        {}};
    if (head.latest_receipt_ref)
      row_.accepted_condition_head.latest_receipt_ref =
          ConditionReceiptRefV2{fresh(head.latest_receipt_ref->result_id)};
    row_.accepted_presence_source = {{fresh(presence.source.domain),
                                      fresh(presence.source.kind),
                                      fresh(presence.source.id)},
                                     presence.schema};
    captured_ = true;
    return common_success();
  }
  bool captured() const noexcept { return captured_; }
};
// 只有原 allocator 新壳和逐条 typed move；异常退出销毁整个候选，不留半个工地。
void append(CheckpointVector<FacilityConstruction> &rows,
            FacilityConstruction &&row) {
  if (rows.size() == rows.max_size())
    fail("ConstructionSourceCapacityExceeded");
  CheckpointVector<FacilityConstruction> next(rows.get_allocator());
  next.reserve(rows.size() + 1);
  for (auto &old : rows)
    next.emplace_back(std::move(old));
  next.emplace_back(std::move(row));
  rows.swap(next);
}
} // namespace

const ActionTimeRegistration &facility_construction_action_time_registration() {
  static const ActionTimeRegistration registration{
      "facility_construction",
      "local_economy",
      "facility_construction",
      1,
      1,
      ActionTimeMode::fixed_duration,
      ActionRemainingMeaning::service_estimate,
      {},
      {}};
  return registration;
}
Outcome<PendingFacilityConstructionStart>
start_facility_construction_in_candidate(
    FacilityConstructionStartWriteContext &x, const StableId &actor,
    const EconomicRuleRef &terms_ref, const MomentOwnerSource &expected_title,
    const FacilitySourceRef &usage, std::span<const CargoSelection> materials) {
  try {
    candidate(x);
    FacilityConstruction row;
    WorkerCapture worker(x.cost.owned_fields, row);
    take(visit_facility_construction_adoptable_worker(
        x.service, x.activity, actor, terms_ref, worker));
    if (!worker.captured())
      fail("ConstructionActualWorkerCaptureRequired", actor);
    const auto *terms =
        take(read_facility_construction_terms(x.cost.read.catalog, terms_ref));
    auto construction_id =
        take(x.cost.common.next_owned_id("facility_construction"));
    const auto consent =
        take(adopt_production_asset_construction_consent_in_candidate(
            x.cost.assets, x.cost.read, actor, terms_ref, expected_title,
            usage));
    const auto cost = take(apply_facility_construction_start_cost_in_candidate(
        x.cost, consent, construction_id, materials));
    const auto *accepted =
        take(read_pending_production_asset_construction_consent(x.cost.assets,
                                                                consent));
    const auto &actual_cost = x.cost.state.start_costs.at(cost.row_index);
    if (actual_cost.construction_id != construction_id.id())
      fail("ConstructionActualStartCostMismatch", construction_id.id());
    const SourceRefBuildView start_source{
        "local_economy", "facility_construction_started", construction_id.id()};
    const SourceRefBuildView permission_source{
        "local_economy", "production_asset_construction_consented",
        accepted->consent_id};


    (x.cost.owned_fields).grow(0);
    row.construction_id = fresh(construction_id.id());
    row.builder_actor_id = fresh(actor);
    row.terms_ref = {fresh(terms->terms_ref.id), terms->terms_ref.version,
                     fresh(terms->terms_ref.catalog_digest)};
    row.execution_binding = {fresh(construction_id.id()),
                             literal("facility_construction"),
                             {literal(start_source.domain),
                              literal(start_source.kind),
                              fresh(construction_id.id())}};
    row.actual_cost_ref = fresh(actual_cost.receipt_id);
    row.authority_sources.reserve(1);
    row.authority_sources.emplace_back(MomentOwnerSource{
        {literal(permission_source.domain), literal(permission_source.kind),
         fresh(accepted->consent_id)},
        1});
    const auto index = x.cost.state.constructions.size();
    append(x.cost.state.constructions, std::move(row));
    take(x.cost.common.append_fact_borrowed(construction_id.id(),
                                            "facility_construction_started",
                                            "local_economy"));


    auto mutation_memory = CheckpointMemoryCharge(0);
    ActionMutation action;
    action.kind = ActionMutation::Kind::insert;
    action.action.id = fresh(construction_id.id());
    action.action.kind = literal("facility_construction");
    action.action.actor_id = fresh(actor);
    action.action.participants.reserve(1);
    action.action.participants.emplace_back(fresh(actor));
    action.action.start_tick =
        x.cost.state.constructions[index].started_at.half_day_tick;
    // Common 的兼容投影由原 Fixed consumer 后续维护，不能以它另计已做工量。
    action.action.remaining_ticks = terms->duration_halfdays;
    take(x.cost.common.apply_action(action));
    ActorMutation occupy;
    occupy.actor_id = fresh(actor);
    occupy.action_id = fresh(construction_id.id());
    take(x.cost.common.apply_actor(occupy));
    const auto &started = x.cost.state.constructions[index];
    auto elapsed = take(begin_fixed_elapsed(
        started.construction_id, started.construction_id, terms->terms_ref.id,
        started.actual_cost_ref, started.started_at, terms->duration_halfdays,
        x.service.scale, x.service.spatial.budget));
    auto inputs = take(read_facility_construction_current_service_inputs(
        x.service, started.construction_id));
    const std::vector<FixedServiceInputRef> no_previous;
    FixedServiceTransitionInput transition{
        FixedServiceAdoptionTransitionBuildView{
            started.construction_id, 1, started.construction_id, 0,
            std::nullopt, FixedTransitionKind::started, started.started_at,
            elapsed.credited_units, start_source, 1,
            FixedOwnerAdoptedBuildView{start_source, 1}, inputs.values(),
            no_previous, std::nullopt}};
    const auto *actual_action = x.cost.common.action(started.construction_id);
    if (!actual_action)
      fail("ConstructionActualExclusiveActionRequired",
           started.construction_id);
    const auto &registration = facility_construction_action_time_registration();
    auto prepared = take(prepare_fixed_service_adoption(
        *actual_action, elapsed, transition.row(), registration,
        x.service.scale, x.action_sources, x.service.spatial.budget));
    take(apply_fixed_service_change_in_candidate(
        x.elapsed, std::move(prepared), registration, x.service.scale,
        x.action_sources, x.service.spatial.budget));
    return {true, {index, consent, cost}, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Status bind_facility_construction_start_command_in_candidate(
    FacilityConstructionStartWriteContext &x,
    const PendingFacilityConstructionStart &pending,
    const CommitResult &result) {
  try {
    candidate(x);
    if (pending.row_index >= x.cost.state.constructions.size() ||
        pending.cost.row_index >= x.cost.state.start_costs.size())
      fail("ConstructionPendingStartRequired");
    const auto &started = x.cost.state.constructions[pending.row_index];
    const auto &cost = x.cost.state.start_costs[pending.cost.row_index];
    if (started.actual_cost_ref != cost.receipt_id ||
        cost.construction_id != started.construction_id ||
        !find_fixed_elapsed(x.elapsed, started.construction_id))
      fail("ConstructionActualFixedAdoptionRequired", started.construction_id);


    auto source_memory = CheckpointMemoryCharge(0);
    const MomentOwnerSource source{{literal("local_economy"),
                                    literal("facility_construction_started"),
                                    fresh(started.construction_id)},
                                   1};
    take(bind_production_asset_construction_command_result_in_candidate(
        x.cost.assets, x.cost.read, pending.consent, source, result));
    take(read_facility_construction_adoption_for_original_dependency(
        x.cost.read, source));
    take(read_facility_construction_current_service_inputs(
        x.service, started.construction_id));
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
