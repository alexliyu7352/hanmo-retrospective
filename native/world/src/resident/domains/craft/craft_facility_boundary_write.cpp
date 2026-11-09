#include "craft_facility_boundary_write.hpp"
#include "craft_control.hpp"
#include "craft_facility_capacity_plan.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace craft_boundary_write {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
// 解包边界写入结果；失败保留原错误并交由本写入单元的捕获边界处理。
template <class T> T take_boundary_write_outcome(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
// 检查边界写入状态；失败沿用本写入单元的原错误传播路径。
void take_boundary_write_outcome(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take_boundary_write_outcome(compare_world_moments(a, b));
}
bool source(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
Integer revision(const FacilityClaimsState&s,const FacilitySlotRef&slot){Integer out=0;for(const auto&h:s.slot_histories)if(h.slot_ref==slot){if(const auto*p=std::get_if<VacantPrunedFacilityPrefix>(&h.base))out=p->last_removed_revision;for(const auto&m:h.mutations)out=m.claim_set_revision;}if(out==std::numeric_limits<Integer>::max())fail("CraftFacilityRevisionOverflow",slot.facility_id);return out;}
void bound(const CraftFacilityBoundaryWriteContext &x, const WorldMoment &at) {
  const auto &o = x.read.occupancy;
  if (&x.common != &o.craft.materials.common ||
      &x.jobs != &o.craft.materials.jobs ||
      &x.items != &o.craft.materials.instances ||
      &x.elapsed != &o.craft.elapsed || &x.craft.read != &o.craft ||
      o.boundaries != &x.read ||
      compare(take_boundary_write_outcome(o.craft.clock.read_current_moment()),
              at) ||
      compare(x.read.physical.current, at) ||
      !x.craft.effects.bound_to(x.common, x.jobs))
    fail("CraftFacilityWriteContextMismatch");
  if (x.repair && (!o.repair || &x.repair->read != o.repair ||
                   &x.repair->read.items != &x.items ||
                   !x.repair->effects.bound_to(x.common, x.items)))
    fail("RepairFacilityWriteContextMismatch");
}
CraftFacilityBusinessSourceView
original(const CraftFacilityBoundaryWriteContext &x, const FacilityClaim2 &c) {
  std::optional<CraftFacilityBusinessSourceView> out;
  auto add = [&](MomentOwnerSource source) {
    if (out)
      fail("AmbiguousCraftFacilityClaimOwner", c.claim_id);
    out = take_boundary_write_outcome(
        read_craft_facility_business_source(x.read.occupancy, source));
  };
  for (const auto &row : x.jobs.jobs.rows())
    if (std::count(row.second.facility_claim_refs.begin(),
                   row.second.facility_claim_refs.end(), c.claim_id) == 1)
      add({craft_job_source(row.first), 1});
  for (const auto &row : x.items.repair_jobs.jobs.rows())
    if (std::count(row.second.claim_refs.begin(), row.second.claim_refs.end(),
                   c.claim_id) == 1)
      add({repair_job_source(row.first), 1});
  if (!out || out->slot_ref != c.slot_ref || out->holder != c.holder ||
      c.units != 1)
    fail("CraftOriginalFacilityClaimRequired", c.claim_id);
  return *out;
}
const EconomicFacilitySlotDefinition& definition(const CraftFacilityBoundaryWriteContext&x,const EconomicRuleRef&r){const EconomicFacilitySlotDefinition*out=nullptr;for(const auto&d:x.read.physical.state.definitions)if(d.definition_ref.id==r.id&&d.definition_ref.version==r.version&&d.definition_ref.catalog_digest==r.catalog_digest){if(out)fail("CraftFacilityDefinitionAmbiguous",r.id);out=&d;}if(!out)fail("CraftFacilityOriginalDefinitionRequired",r.id);return *out;}
std::optional<CraftFacilityOccupancyView>
placement(const CraftFacilityBoundaryWriteContext &x,
          const CraftFacilityBusinessSourceView &o) {
  const auto &refs = o.craft_job ? o.craft_job->facility_occupancy_event_refs
                                 : o.repair_job->facility_occupancy_event_refs;
  if (refs.empty())
    return {};
  auto result = take_boundary_write_outcome(read_craft_facility_occupancy(
      x.read.occupancy, craft_facility_occupancy_source(refs.back()), 1));
  if (result.slot_ref != o.slot_ref || result.holder != o.holder)
    fail("CraftFacilityPlacementOwnerMismatch", refs.back());
  return result;
}
bool retained(const CraftFacilityBoundaryWriteContext &x,
              const FacilityClaim2 &c) {
  if (c.source_result_ref.kind != "facility_retention")
    return false;
  const auto *t =
      take_boundary_write_outcome(read_facility_retention_transition(
          x.read.physical.state, {c.source_result_ref, 1}, c.slot_ref));
  Integer count = 0;
  bool out = false;
  for (const auto &e : t->entries)
    std::visit(
        [&](const auto &v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, FacilityRetentionBegin> ||
                        std::is_same_v<T, FacilityRetentionContinue>) {
            if (v.retained_claim_id == c.claim_id) {
              ++count;
              out = true;
            }
          } else if constexpr (std::is_same_v<
                                   T, FacilityRetentionRestoredService>) {
            if (v.normal_claim_id == c.claim_id)
              ++count;
          }
        },
        e);
  if (count != 1)
    fail("CraftFacilityRetentionIdentityMismatch", c.claim_id);
  return out;
}
using Prior=craft_capacity_plan::Prior;
std::vector<Prior> prior(const CraftFacilityBoundaryWriteContext &x,
                         const FacilityCapacityReconciliationRequest &q,
                         const FacilityReconciliationProgress &p) {
  std::vector<Prior> out;
  if (p.position >= p.ordered_claims.size() ||
      p.prior_owner_results.size() != p.position)
    fail("CraftFacilityReconciliationProgressMismatch");
  for (std::size_t n = 0; n < p.position; ++n) {
    const auto &old = p.ordered_claims[n];
    const auto &r = p.prior_owner_results[n];
    if (old.slot_ref != q.slot_ref ||
        r.claim_set_revision !=
            revision(x.read.occupancy.claims, q.slot_ref) + 1 ||
        compare(r.occurred_at, q.occurred_at))
      fail("CraftPriorFacilityResultMismatch", old.claim_id);
    bool closed = false;
    for (const auto &v : r.closed_claims) {
      if (v.claim_id != old.claim_id || closed)
        fail("CraftPriorFacilityCloseMismatch", old.claim_id);
      closed = true;
    }
    take_boundary_write_outcome(
        x.read.physical.holders.validate_holder_mutation_source(q.slot_ref, r));
    std::optional<FacilityRetentionEntry> entry;
    if (closed)
      entry = take_boundary_write_outcome(
          x.read.physical.applied.read_original_facility_retention_disposition(
              old, r));
    else
      out.push_back({{old.from, old.until},
                     old.units,
                     retained(x, old),
                     take_boundary_write_outcome(
                         read_craft_facility_claim_units(x.read, old))});
    for (const auto &c : r.admitted_claims) {
      if (c.slot_ref != old.slot_ref || c.holder != old.holder)
        fail("CraftPriorFacilityAdmissionMismatch", c.claim_id);
      bool keep = false;
      if (entry)
        std::visit(
            [&](const auto &v) {
              using T = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<T, FacilityRetentionBegin> ||
                            std::is_same_v<T, FacilityRetentionContinue>)
                if (v.retained_claim_id == c.claim_id)
                  keep = true;
            },
            *entry);
      out.push_back(
          {{c.from, c.until},
           c.units,
           keep,
           take_boundary_write_outcome(
               x.read.physical.applied.read_original_facility_admission_units(
                   c, r))});
    }
  }
  return out;
}
// This private adapter participates only in a real capacity review. It defers
// that one pause's physical disposition to the same ordered original result;
// every other business effect remains with the original concrete owner.
class DeferredCraftPause final:public CraftJobEffectsWrite {
 CraftFacilityBoundaryWriteContext&x_;const SourceRef&cause_;const StableId&job_;
public:DeferredCraftPause(CraftFacilityBoundaryWriteContext&x,const SourceRef&cause,const StableId&job):x_(x),cause_(cause),job_(job){}
 bool bound_to(const CommonStore&c,const CraftJobsState&s)const noexcept override{return &c==&x_.common&&&s==&x_.jobs;}
 Outcome<std::vector<StableId>> adopt_craft_facility_claims_in_candidate(const CraftJob&j)override{return x_.craft.effects.adopt_craft_facility_claims_in_candidate(j);}
 Outcome<CraftGateEffects> apply_actual_craft_gate_effects_in_candidate(const CraftJob&j,const SourceRef&s,const WorldMoment&m)override{return x_.craft.effects.apply_actual_craft_gate_effects_in_candidate(j,s,m);}
 Outcome<CraftControlEffects>
apply_craft_control_in_candidate(const CraftJob &j,
                                 const CraftControlSource &c) override {
  if (j.id != job_ || c.kind != CraftControlKind::pause || !c.actual_cause ||
      !same_source_ref(c.actual_cause->source_ref, cause_) ||
      compare(c.occurred_at, take_boundary_write_outcome(
                                 x_.craft.read.clock.read_current_moment())))
    return {false,
            {},
            common_failure("CraftDeferredPauseSourceMismatch", j.id,
                           ErrorKind::internal)
                .error};
  return {true, {}, {}};
}
 Outcome<std::vector<SourceRef>> apply_actual_craft_completion_in_candidate(const CraftJob&j,const SourceRef&s,const WorldMoment&m)override{return x_.craft.effects.apply_actual_craft_completion_in_candidate(j,s,m);}
};
class DeferredRepairPause final:public RepairEffectsWrite {
 CraftFacilityBoundaryWriteContext&x_;const SourceRef&cause_;const StableId&job_;
public:DeferredRepairPause(CraftFacilityBoundaryWriteContext&x,const SourceRef&cause,const StableId&job):x_(x),cause_(cause),job_(job){}
 bool bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept override{return &c==&x_.common&&&i==&x_.items;}
 Outcome<std::vector<StableId>> adopt_repair_claims_in_candidate(const RepairJob&j)override{return x_.repair->effects.adopt_repair_claims_in_candidate(j);}
 Outcome<std::vector<StableId>> apply_repair_control_claims_in_candidate(
    const RepairJob &j, const RepairControlSource &c) override {
  if (j.id != job_ || c.kind != RepairControlKind::suspended ||
      !same_source_ref(c.actual_cause.source_ref, cause_) ||
      compare(c.occurred_at, take_boundary_write_outcome(
                                 x_.repair->read.clock.read_current_moment())))
    return {false,
            {},
            common_failure("RepairDeferredPauseSourceMismatch", j.id,
                           ErrorKind::internal)
                .error};
  return {true, {}, {}};
}
 Status apply_repair_terminal_facility_disposition_in_candidate(const RepairJob&j,const SourceRef&s,const WorldMoment&m)override{return x_.repair->effects.apply_repair_terminal_facility_disposition_in_candidate(j,s,m);}
};
std::optional<MomentOwnerSource>
pause(CraftFacilityBoundaryWriteContext &x,
      const CraftFacilityBusinessSourceView &o, const FacilitySourceRef &cause,
      const FacilityClaimUntilRequest *until = nullptr) {
  if (o.craft_job) {
    if (o.craft_job->phase != CraftJobPhase::working)
      return {};
    const auto *work = take_boundary_write_outcome(
        read_craft_actual_work(x.craft.read, *o.craft_job));
    const auto *gate = std::get_if<CraftWorkGate>(&work->gate);
    if (!gate)
      fail("CraftFacilityGateRequired", o.craft_job->id);
    DeferredCraftPause deferred(x, cause.source_ref, o.craft_job->id);
    auto write = x.craft;
    CraftJobWriteContext bound{
        write.read, write.action_sources,       write.original_periods,
        deferred,   write.cancellation_effects, write.condition_effects};
    CraftControlRequest request{
        o.craft_job->id,
        {},
        CraftControlKind::pause,
        static_cast<Integer>(o.craft_job->control_refs.size()),
        gate->gate_ordinal,
        gate->last_closed_gate_result_ref,
        MomentOwnerSource{cause.source_ref, cause.source_schema}};
    const auto id =
        until ? take_boundary_write_outcome(
                    apply_craft_facility_until_pause_in_candidate(
                        x.common, x.jobs, x.elapsed, bound, request, *until))
              : take_boundary_write_outcome(apply_craft_control_in_candidate(
                    x.common, x.jobs, x.elapsed, bound, request));
    return MomentOwnerSource{craft_control_source(id), 1};
  }
  if (o.repair_job->phase != RepairJobPhase::working)
    return {};
  if (!x.repair)
    fail("RepairFacilityWriterRequired", o.repair_job->id,
         ErrorKind::unsupported_capability);
  DeferredRepairPause deferred(x, cause.source_ref, o.repair_job->id);
  RepairWriteContext write{x.repair->read,
                           deferred,
                           x.repair->stamps,
                           x.repair->stamp_authority,
                           x.repair->versions,
                           x.repair->facility_occupancy_owner,
                           x.repair->original_economy};
  const auto id =
      until ? take_boundary_write_outcome(
                  apply_repair_facility_until_pause_in_candidate(
                      x.common, x.items, x.elapsed, write, o.repair_job->id,
                      *until))
            : take_boundary_write_outcome(apply_repair_control_in_candidate(
                  x.common, x.items, x.elapsed, write, o.repair_job->id,
                  RepairControlKind::suspended,
                  {cause.source_ref, cause.source_schema}));
  return MomentOwnerSource{repair_control_source(id), 1};
}
FacilityClaimMutationReceipt receipt(const CraftFacilityBoundaryWriteContext&x,const CraftFacilityBoundarySource&r,const FacilityClaim2&old,const EconomicFacilityProspectiveCapacity&cap){FacilityClaimMutationReceipt out;out.claim_set_revision=revision(x.read.occupancy.claims,old.slot_ref)+1;out.occurred_at=r.occurred_at;out.source_result_ref=craft_facility_boundary_source(r.id);out.source_schema=1;if(!std::holds_alternative<CraftFacilityUnchangedService>(r.disposition))out.closed_claims.push_back({old.claim_id,std::holds_alternative<CraftFacilitySuspendedWithoutPlacement>(r.disposition)?FacilityClaimCloseReason::suspended:FacilityClaimCloseReason::replaced});std::visit([&](const auto&d){using T=std::decay_t<decltype(d)>;if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>||std::is_same_v<T,CraftFacilityRetainedPlacement>){const auto&id=[&]()->const StableId&{if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>)return d.new_claim_id;else return d.retained_claim_id;}();const auto&window=[&]()->const MomentSpan&{if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>)return d.accepted_window;else return d.accepted_review_window;}();out.admitted_claims.push_back({id,old.slot_ref,old.holder,window.from,window.to,old.units,cap.capacity_revision,std::is_same_v<T,CraftFacilityRetainedPlacement>?old.usage_right_ref:cap.usage_right_ref,out.source_result_ref});}},r.disposition);return out;}
}
using namespace craft_boundary_write;
Outcome<FacilityClaimMutationReceipt>
apply_craft_facility_capacity_boundary_in_candidate(
    CraftFacilityBoundaryWriteContext &x,
    const FacilityCapacityReconciliationRequest &q,
    const FacilityReconciliationProgress &p) {
  try {
    bound(x, q.occurred_at);
    if (p.position >= p.ordered_claims.size())
      fail("CraftFacilityReconciliationProgressMismatch");
    const auto old = p.ordered_claims[p.position];
    const auto o = original(x, old);
    if (o.repair_job && !x.repair)
      fail("RepairFacilityWriterRequired", o.repair_job->id,
           ErrorKind::unsupported_capability);
    if (old.slot_ref != q.slot_ref || compare(old.until, q.occurred_at) < 0)
      fail("CraftFacilityActualClaimMismatch", old.claim_id);
    const auto *head =
        x.common.state().facts.find(q.original_change.source_ref.id);
    if (!head || head->domain != q.original_change.source_ref.domain ||
        head->kind != q.original_change.source_ref.kind)
      fail("CraftFacilityIndependentChangeRequired",
           q.original_change.source_ref.id);
    for (const auto &entry : x.jobs.facility_boundaries.rows())
      if (const auto *c =
              std::get_if<CraftFacilityCapacityChanged>(&entry.second.cause);
          c &&
          c->actual_change.source_schema == q.original_change.source_schema &&
          same_source_ref(c->actual_change.source_ref,
                          q.original_change.source_ref)) {
        const auto &stored = entry.second;
        const auto previous = std::visit(
            [](const auto &d) -> const StableId & {
              using T = std::decay_t<decltype(d)>;
              if constexpr (std::is_same_v<T, CraftFacilityUnchangedService>)
                return d.claim_id;
              else
                return d.previous_claim_id;
            },
            stored.disposition);
        if (previous != old.claim_id)
          continue;
        take_boundary_write_outcome(
            read_craft_facility_boundary_for_physical_effect(
                x.read, {craft_facility_boundary_source(stored.id), 1}));
        for (const auto &h : x.read.occupancy.claims.slot_histories)
          if (h.slot_ref == q.slot_ref)
            for (const auto &m : h.mutations)
              if (!compare(m.occurred_at, stored.occurred_at)) {
                const auto match = take_boundary_write_outcome(
                    read_craft_facility_boundary_for_claim(x.read, old, m));
                if (!match || match->original->id != stored.id)
                  continue;
                FacilityClaimMutationReceipt out;
                out.claim_set_revision = m.claim_set_revision;
                out.occurred_at = m.occurred_at;
                out.source_result_ref =
                    craft_facility_boundary_source(stored.id);
                out.source_schema = 1;
                for (const auto &closed : m.closed_claims)
                  if (closed.claim_id == old.claim_id)
                    out.closed_claims.push_back(closed);
                std::visit(
                    [&](const auto &d) {
                      using T = std::decay_t<decltype(d)>;
                      if constexpr (std::is_same_v<
                                        T, CraftFacilityReadmittedService> ||
                                    std::is_same_v<
                                        T, CraftFacilityRetainedPlacement>) {
                        const auto &id = [&]() -> const StableId & {
                          if constexpr (std::is_same_v<
                                            T, CraftFacilityReadmittedService>)
                            return d.new_claim_id;
                          else
                            return d.retained_claim_id;
                        }();
                        for (auto admitted : m.admitted_claims)
                          if (admitted.claim_id == id) {
                            admitted.source_result_ref = out.source_result_ref;
                            out.admitted_claims.push_back(std::move(admitted));
                          }
                      }
                    },
                    stored.disposition);
                return {true, std::move(out), {}};
              }
        MomentSpan replay_window{stored.occurred_at, old.until};
        std::visit(
            [&](const auto &d) {
              using T = std::decay_t<decltype(d)>;
              if constexpr (std::is_same_v<T, CraftFacilityReadmittedService>)
                replay_window = d.accepted_window;
              else if constexpr (std::is_same_v<T,
                                                CraftFacilityRetainedPlacement>)
                replay_window = d.accepted_review_window;
            },
            stored.disposition);
        if (compare(replay_window.from, replay_window.to) >= 0)
          fail("CraftBoundaryReplayWindowRequired", stored.id);
        const auto cap = take_boundary_write_outcome(
            read_prospective_economic_facility_capacity(
                x.read.physical, q.slot_ref, q.original_change, replay_window));
        return {true, receipt(x, stored, old, cap), {}};
      }
    const auto earlier = prior(x, q, p);
    const auto occupied = placement(x, o);
    const bool working = o.craft_job
                             ? o.craft_job->phase == CraftJobPhase::working
                             : o.repair_job->phase == RepairJobPhase::working;
    auto eligibility =
        o.craft_job
            ? take_boundary_write_outcome(
                  x.craft.read.original.read_current_craft_facility_eligibility(
                      *o.craft_job))
            : take_boundary_write_outcome(
                  x.repair->read.original
                      .read_current_repair_facility_eligibility(*o.repair_job));
    const auto *eligible = std::get_if<CraftFacilityEligible>(&eligibility);
    CraftFacilityBoundarySource row;
    row.job_or_repair_ref = o.original_owner;
    row.occurred_at = q.occurred_at;
    row.cause = CraftFacilityCapacityChanged{q.original_change};
    row.original_terms_ref =
        o.craft_job ? MomentOwnerSource{o.craft_job->terms.acceptance_ref, 1}
                    : o.repair_job->terms.accepted_service_ref;
    row.adopted_input_refs =
        std::visit([](const auto &v) { return v.actual_inputs; }, eligibility);
    if (const auto *b = std::get_if<CraftFacilityActuallyBlocked>(&eligibility);
        b && std::none_of(row.adopted_input_refs.begin(),
                          row.adopted_input_refs.end(), [&](const auto &s) {
                            return source(s, b->actual_blocking_source);
                          }))
      fail("CraftFacilityActualBlockingSourceRequired", old.claim_id);
    const auto actual_definition = take_boundary_write_outcome(
        read_economic_facility_definition_after_original_change(
            x.read.physical, q.slot_ref, q.original_change));
    MomentSpan window{q.occurred_at, old.until};
    if (compare(window.from, window.to) >= 0) {
      if (actual_definition.retention_review_halfdays < 1 ||
          actual_definition.retention_review_halfdays >
              actual_definition.max_claim_half_days ||
          window.from.half_day_tick >
              std::numeric_limits<Integer>::max() -
                  actual_definition.retention_review_halfdays)
        fail("CraftFacilityRetentionWindowRequired", old.claim_id);
      window.to = window.from;
      window.to.half_day_tick += actual_definition.retention_review_halfdays;
    }
    if (eligible && eligible->accepted_service_window &&
        compare(eligible->accepted_service_window->to, window.to) > 0)
      window.to = eligible->accepted_service_window->to;
    const auto cap =
        take_boundary_write_outcome(read_prospective_economic_facility_capacity(
            x.read.physical, q.slot_ref, q.original_change, window));
    const auto &def = definition(x, cap.definition_ref);
    const auto old_units = take_boundary_write_outcome(
        read_craft_facility_claim_units(x.read, old));
    const auto whole_restriction = take_boundary_write_outcome(
        x.read.physical.applied.read_applied_whole_slot_restriction(
            q.original_change, q.slot_ref));
    bool whole_closed = whole_restriction.has_value();
    if (!std::holds_alternative<IndependentFacilityUnits>(
            def.physical_layout)) {
      Integer actually_occupied = 0;
      for (const auto &other : p.ordered_claims) {
        if (compare(other.from, q.occurred_at) > 0)
          continue;
        const auto holder = original(x, other);
        const auto contents = placement(x, holder);
        if (!contents || !contents->occupied_units)
          continue;
        if (contents->occupied_units != other.units)
          fail("CraftRetentionActualOccupancyMismatch", other.claim_id);
        if (actually_occupied >
            std::numeric_limits<Integer>::max() - other.units)
          fail("CraftFacilityOccupancyOverflow", other.claim_id);
        actually_occupied += other.units;
      }
      // In a shared safety zone, actual uncleared WIP is the original reason
      // that no complete normal job can safely coexist. Future reservations are
      // not WIP.
      whole_closed = whole_closed || actually_occupied > cap.capacity;
    }
    std::vector<FacilityPhysicalUnitRef> later_placements;
    for (std::size_t n = p.position + 1; n < p.ordered_claims.size(); ++n) {
      const auto &later = p.ordered_claims[n];
      if (!take_boundary_write_outcome(
              x.read.physical.holders.has_actual_facility_service_started(
                  later, q.occurred_at)))
        continue;
      auto units = take_boundary_write_outcome(
          read_craft_facility_claim_units(x.read, later));
      later_placements.insert(later_placements.end(), units.begin(),
                              units.end());
    }
    const auto decision =
        take_boundary_write_outcome(craft_capacity_plan::choose(
            {old, q.occurred_at, cap, def, eligible, working, retained(x, old),
             occupied && occupied->occupied_units > 0,
             occupied && occupied->occupied_units == 0, whole_closed, old_units,
             earlier, later_placements,
             x.read.claim_catalog.max_claim_half_days}));
    switch (decision.kind) {
    case craft_capacity_plan::Disposition::unchanged:
      row.disposition = CraftFacilityUnchangedService{old.claim_id};
      break;
    case craft_capacity_plan::Disposition::cleared:
      row.disposition = CraftFacilityClearedPlacement{
          old.claim_id, {occupied->source_ref, occupied->source_schema}};
      break;
    case craft_capacity_plan::Disposition::readmitted:
      row.disposition = CraftFacilityReadmittedService{
          old.claim_id,
          take_boundary_write_outcome(x.common.next_id("facility_claim")),
          *decision.window, decision.physical_units};
      break;
    case craft_capacity_plan::Disposition::retained:
    case craft_capacity_plan::Disposition::suspended: {
      const auto actual_control = pause(x, o, q.original_change);
      if (actual_control)
        row.adopted_input_refs.push_back(*actual_control);
      if (decision.kind == craft_capacity_plan::Disposition::retained)
        row.disposition = CraftFacilityRetainedPlacement{
            old.claim_id,
            take_boundary_write_outcome(x.common.next_id("facility_claim")),
            *decision.window,
            {occupied->source_ref, occupied->source_schema},
            actual_control};
      else {
        if (!actual_control)
          fail("CraftFacilityOriginalSuspensionRequired", old.claim_id,
               ErrorKind::unsupported_capability);
        row.disposition = CraftFacilitySuspendedWithoutPlacement{
            old.claim_id, *actual_control};
      }
      break;
    }
    }
    // The producer has completed the genuine old-service/control work before
    // the immutable adoption head. A failing finalizer discards this entire
    // candidate.
    row.id = take_boundary_write_outcome(
        x.common.next_id("craft_facility_boundary"));
    for (const auto &id : x.common.state().facts.order()) {
      const auto *previous = x.jobs.facility_boundaries.find(id);
      if (!previous ||
          previous->job_or_repair_ref.index() != row.job_or_repair_ref.index())
        continue;
      bool same = false;
      if (const auto *v =
              std::get_if<CraftOccupancyJobRef>(&row.job_or_repair_ref))
        same = std::get<CraftOccupancyJobRef>(previous->job_or_repair_ref)
                   .job_ref == v->job_ref;
      else
        same =
            std::get<CraftOccupancyRepairRef>(previous->job_or_repair_ref)
                .repair_ref ==
            std::get<CraftOccupancyRepairRef>(row.job_or_repair_ref).repair_ref;
      if (same)
        row.previous_boundary_ref = id;
    }
    const auto result = receipt(x, row, old, cap);
    for (const auto &admitted : result.admitted_claims) {
      if (o.craft_job)
        x.jobs.jobs.find(o.craft_job->id)
            ->facility_claim_refs.push_back(admitted.claim_id);
      else
        x.items.repair_jobs.jobs.find(o.repair_job->id)
            ->claim_refs.push_back(admitted.claim_id);
    }
    const auto id = row.id;
    take_boundary_write_outcome(
        x.common.append_fact({id, "craft_facility_boundary", "crafting"}));
    if (!x.jobs.facility_boundaries.insert(id, std::move(row)))
      fail("DuplicateCraftFacilityBoundary", id);
    take_boundary_write_outcome(
        read_craft_facility_boundary_for_physical_effect(
            x.read, {craft_facility_boundary_source(id), 1}));
    NativeCraftFacilityHolderRead holder(x.read.occupancy);
    take_boundary_write_outcome(
        holder.validate_holder_mutation_source(q.slot_ref, result));
    return {true, result, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
namespace craft_boundary_write {
PreparedFacilityClaimMutation prepared_receipt(const CraftFacilityBoundaryWriteContext&x,const CraftFacilityBoundarySource&r,const FacilityClaim2&old,const EconomicFacilityServiceCapacityView&cap){
 PreparedFacilityClaimMutation out;out.claim_set_revision=revision(x.read.occupancy.claims,old.slot_ref)+1;out.occurred_at=r.occurred_at;out.source_result_ref=craft_facility_boundary_source(r.id);out.source_schema=1;
 if(!std::holds_alternative<CraftFacilityUnchangedService>(r.disposition))out.closed_claims.push_back({old.claim_id,std::holds_alternative<CraftFacilitySuspendedWithoutPlacement>(r.disposition)?FacilityClaimCloseReason::suspended:FacilityClaimCloseReason::replaced});
 std::visit([&](const auto&d){using T=std::decay_t<decltype(d)>;if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>||std::is_same_v<T,CraftFacilityRetainedPlacement>){const auto&id=[&]()->const StableId&{if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>)return d.new_claim_id;else return d.retained_claim_id;}();const auto&w=[&]()->const MomentSpan&{if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>)return d.accepted_window;else return d.accepted_review_window;}();out.admitted_claims.push_back({id,old.slot_ref,old.holder,w.from,w.to,old.units,std::is_same_v<T,CraftFacilityRetainedPlacement>?old.usage_right_ref:cap.usage_right_ref,out.source_result_ref});}},r.disposition);return out;
}
std::vector<Prior> until_prior(const CraftFacilityBoundaryWriteContext &x,
                               const FacilityClaimUntilRequest &q,
                               const FacilityUntilReconciliationProgress &p) {
  if (p.position >= p.ordered_claims.size() ||
      p.prior_owner_results.size() != p.position)
    fail("CraftUntilProgressMismatch");
  NativeCraftPreparedFacilityRead original(x.read);
  std::vector<Prior> out;
  for (std::size_t n = 0; n < p.position; ++n) {
    const auto &old = p.ordered_claims[n];
    const auto &r = p.prior_owner_results[n];
    if (old.slot_ref != q.due_claim.slot_ref ||
        r.claim_set_revision !=
            revision(x.read.occupancy.claims, old.slot_ref) + 1 ||
        compare(r.occurred_at, q.occurred_at))
      fail("CraftUntilPriorResultMismatch", old.claim_id);
    take_boundary_write_outcome(
        original.validate_prepared_facility_disposition(old.slot_ref, r));
    bool closed = false;
    for (const auto &v : r.closed_claims) {
      if (v.claim_id != old.claim_id || closed)
        fail("CraftUntilPriorCloseMismatch", old.claim_id);
      closed = true;
    }
    std::optional<FacilityRetentionEntry> entry;
    if (closed)
      entry = take_boundary_write_outcome(
          original.read_prepared_facility_retention(old, r));
    else
      out.push_back({{old.from, old.until},
                     old.units,
                     retained(x, old),
                     take_boundary_write_outcome(
                         read_craft_facility_claim_units(x.read, old))});
    for (const auto &c : r.admitted_claims) {
      bool keep = false;
      if (entry)
        std::visit(
            [&](const auto &v) {
              using T = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<T, FacilityRetentionBegin> ||
                            std::is_same_v<T, FacilityRetentionContinue>)
                if (v.retained_claim_id == c.claim_id)
                  keep = true;
            },
            *entry);
      out.push_back(
          {{c.from, c.until},
           c.units,
           keep,
           take_boundary_write_outcome(
               original.read_prepared_facility_admission_units(c, r))});
    }
  }
  return out;
}
}
Outcome<PreparedFacilityClaimMutation>
apply_craft_facility_until_boundary_in_candidate(
    CraftFacilityBoundaryWriteContext &x, const FacilityClaimUntilRequest &q,
    const FacilityUntilReconciliationProgress &p) {
  try {
    bound(x, q.occurred_at);
    if (p.position >= p.ordered_claims.size() ||
        compare(q.due_claim.until, q.occurred_at))
      fail("CraftUntilActualMomentMismatch");
    const auto due = take_boundary_write_outcome(read_facility_claim_episode(
        x.read.occupancy.claims, q.due_claim.claim_id, q.occurred_at));
    if (due.close_receipt_ref ||
        due.admission_receipt_ref.slot_ref !=
            q.admission_receipt_ref.slot_ref ||
        due.admission_receipt_ref.claim_set_revision !=
            q.admission_receipt_ref.claim_set_revision ||
        due.original_claim.slot_ref != q.due_claim.slot_ref ||
        compare(due.original_claim.until, q.occurred_at))
      fail("CraftUntilOriginalEpisodeRequired", q.due_claim.claim_id);
    const auto old = p.ordered_claims[p.position];
    const auto o = original(x, old);
    if (old.slot_ref != q.due_claim.slot_ref ||
        compare(old.until, q.occurred_at) < 0)
      fail("CraftUntilAffectedClaimMismatch", old.claim_id);
    if (o.repair_job && !x.repair)
      fail("RepairFacilityWriterRequired", o.repair_job->id,
           ErrorKind::unsupported_capability);
    NativeEconomicFacilitySourceRead capacity_reader(x.read.physical);
    const auto current_capacity = take_boundary_write_outcome(
        capacity_reader.capacity_source_at(old.slot_ref, q.occurred_at));
    const auto def = take_boundary_write_outcome(
        capacity_reader.read_facility_slot_definition_at_source(
            current_capacity, old.slot_ref));
    auto until = q.occurred_at;
    if (def.retention_review_halfdays < 1 ||
        until.half_day_tick >
            std::numeric_limits<Integer>::max() - def.retention_review_halfdays)
      fail("CraftUntilRetentionWindowRequired", old.claim_id);
    until.half_day_tick += def.retention_review_halfdays;
    if (compare(old.until, until) > 0)
      until = old.until;
    for (const auto &entry : x.jobs.facility_boundaries.rows()) {
      const auto *u = std::get_if<CraftFacilityClaimUntil>(&entry.second.cause);
      if (!u || u->claim_id != q.due_claim.claim_id ||
          u->admission_receipt_ref.slot_ref !=
              q.admission_receipt_ref.slot_ref ||
          u->admission_receipt_ref.claim_set_revision !=
              q.admission_receipt_ref.claim_set_revision)
        continue;
      const auto &r = entry.second;
      const auto previous = std::visit(
          [](const auto &d) -> const StableId & {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, CraftFacilityUnchangedService>)
              return d.claim_id;
            else
              return d.previous_claim_id;
          },
          r.disposition);
      if (previous != old.claim_id)
        continue;
      const auto original = take_boundary_write_outcome(
          read_craft_facility_boundary_for_physical_effect(
              x.read, {craft_facility_boundary_source(r.id), 1}));
      std::visit(
          [&](const auto &d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, CraftFacilityReadmittedService>)
              until = d.accepted_window.to;
            else if constexpr (std::is_same_v<T,
                                              CraftFacilityRetainedPlacement>)
              until = d.accepted_review_window.to;
          },
          r.disposition);
      const auto cap = take_boundary_write_outcome(
          read_economic_facility_capacity_before_owner_source(
              x.read.physical, old.slot_ref,
              craft_facility_boundary_source(r.id), {q.occurred_at, until}));
      return {true, prepared_receipt(x, r, old, cap), {}};
    }
    const auto earlier = until_prior(x, q, p);
    const auto occupied = placement(x, o);
    const bool working = o.craft_job
                             ? o.craft_job->phase == CraftJobPhase::working
                             : o.repair_job->phase == RepairJobPhase::working;
    auto eligibility =
        o.craft_job
            ? take_boundary_write_outcome(
                  x.craft.read.original.read_current_craft_facility_eligibility(
                      *o.craft_job))
            : take_boundary_write_outcome(
                  x.repair->read.original
                      .read_current_repair_facility_eligibility(*o.repair_job));
    const auto *eligible = std::get_if<CraftFacilityEligible>(&eligibility);
    if (eligible && eligible->accepted_service_window &&
        compare(eligible->accepted_service_window->to, until) > 0)
      until = eligible->accepted_service_window->to;
    const auto cap =
        take_boundary_write_outcome(read_economic_facility_existing_capacity(
            x.read.physical, old.slot_ref, current_capacity,
            {q.occurred_at, until}));
    const auto old_units = take_boundary_write_outcome(
        read_craft_facility_claim_units(x.read, old));
    bool whole_closed = false;
    if (!std::holds_alternative<IndependentFacilityUnits>(
            def.physical_layout)) {
      Integer count = 0;
      for (const auto &other : p.ordered_claims) {
        if (compare(other.from, q.occurred_at) > 0)
          continue;
        const auto contents = placement(x, original(x, other));
        if (!contents || !contents->occupied_units)
          continue;
        if (contents->occupied_units != other.units ||
            count > std::numeric_limits<Integer>::max() - other.units)
          fail("CraftUntilActualOccupancyMismatch", other.claim_id);
        count += other.units;
      }
      whole_closed = count > cap.capacity;
    }
    // 原 whole-zone 限制不能因普通到期消失，读取当前真容量的 admission_mode。
    const EconomicFacilityCapacitySource *capacity_row = nullptr;
    for (const auto &row : x.read.physical.state.capacity_sources)
      if (row.source_id == current_capacity.source_ref.id)
        capacity_row = &row;
    if (!capacity_row)
      fail("CraftUntilCapacitySourceRequired", old.claim_id);
    if (const auto *closed = std::get_if<EntireSlotNoNewAdmission>(
            &capacity_row->admission_mode)) {
      const auto blocker = take_boundary_write_outcome(
          x.read.physical.applied.read_applied_whole_slot_restriction(
              {closed->whole_slot_restriction_source_ref,
               closed->whole_slot_restriction_source_schema},
              old.slot_ref));
      whole_closed = whole_closed || blocker.has_value();
    }
    std::vector<FacilityPhysicalUnitRef> later;
    for (std::size_t n = p.position + 1; n < p.ordered_claims.size(); ++n)
      if (take_boundary_write_outcome(
              x.read.physical.holders.has_actual_facility_service_started(
                  p.ordered_claims[n], q.occurred_at))) {
        auto units = take_boundary_write_outcome(
            read_craft_facility_claim_units(x.read, p.ordered_claims[n]));
        later.insert(later.end(), units.begin(), units.end());
      }
    const auto decision =
        take_boundary_write_outcome(craft_capacity_plan::choose(
            {old, q.occurred_at, cap, def, eligible, working, retained(x, old),
             occupied && occupied->occupied_units > 0,
             occupied && occupied->occupied_units == 0, whole_closed, old_units,
             earlier, later, x.read.claim_catalog.max_claim_half_days}));
    CraftFacilityBoundarySource row;
    row.job_or_repair_ref = o.original_owner;
    row.occurred_at = q.occurred_at;
    row.cause =
        CraftFacilityClaimUntil{q.due_claim.claim_id, q.admission_receipt_ref};
    row.original_terms_ref =
        o.craft_job ? MomentOwnerSource{o.craft_job->terms.acceptance_ref, 1}
                    : o.repair_job->terms.accepted_service_ref;
    row.adopted_input_refs =
        std::visit([](const auto &v) { return v.actual_inputs; }, eligibility);
    const auto original_admission =
        take_boundary_write_outcome(read_facility_claim_receipt(
            x.read.occupancy.claims, q.admission_receipt_ref));
    const FacilitySourceRef until_cause{original_admission.source_result_ref,
                                        original_admission.source_schema};
    switch (decision.kind) {
    case craft_capacity_plan::Disposition::unchanged:
      row.disposition = CraftFacilityUnchangedService{old.claim_id};
      break;
    case craft_capacity_plan::Disposition::cleared:
      row.disposition = CraftFacilityClearedPlacement{
          old.claim_id, {occupied->source_ref, occupied->source_schema}};
      break;
    case craft_capacity_plan::Disposition::readmitted:
      row.disposition = CraftFacilityReadmittedService{
          old.claim_id,
          take_boundary_write_outcome(x.common.next_id("facility_claim")),
          *decision.window, decision.physical_units};
      break;
    case craft_capacity_plan::Disposition::retained:
    case craft_capacity_plan::Disposition::suspended: {
      const auto paused = pause(x, o, until_cause, &q);
      if (paused)
        row.adopted_input_refs.push_back(*paused);
      if (decision.kind == craft_capacity_plan::Disposition::retained)
        row.disposition = CraftFacilityRetainedPlacement{
            old.claim_id,
            take_boundary_write_outcome(x.common.next_id("facility_claim")),
            *decision.window,
            {occupied->source_ref, occupied->source_schema},
            paused};
      else {
        if (!paused)
          fail("CraftUntilActualSuspensionRequired", old.claim_id,
               ErrorKind::unsupported_capability);
        row.disposition =
            CraftFacilitySuspendedWithoutPlacement{old.claim_id, *paused};
      }
      break;
    }
    }
    row.id = take_boundary_write_outcome(
        x.common.next_id("craft_facility_boundary"));
    for (const auto &id : x.common.state().facts.order()) {
      const auto *previous = x.jobs.facility_boundaries.find(id);
      if (!previous ||
          previous->job_or_repair_ref.index() != row.job_or_repair_ref.index())
        continue;
      bool same = false;
      if (const auto *v =
              std::get_if<CraftOccupancyJobRef>(&row.job_or_repair_ref))
        same = std::get<CraftOccupancyJobRef>(previous->job_or_repair_ref)
                   .job_ref == v->job_ref;
      else
        same =
            std::get<CraftOccupancyRepairRef>(previous->job_or_repair_ref)
                .repair_ref ==
            std::get<CraftOccupancyRepairRef>(row.job_or_repair_ref).repair_ref;
      if (same)
        row.previous_boundary_ref = id;
    }
    const auto result = prepared_receipt(x, row, old, cap);
    for (const auto &c : result.admitted_claims) {
      if (o.craft_job)
        x.jobs.jobs.find(o.craft_job->id)
            ->facility_claim_refs.push_back(c.claim_id);
      else
        x.items.repair_jobs.jobs.find(o.repair_job->id)
            ->claim_refs.push_back(c.claim_id);
    }
    const auto id = row.id;
    take_boundary_write_outcome(
        x.common.append_fact({id, "craft_facility_boundary", "crafting"}));
    if (!x.jobs.facility_boundaries.insert(id, std::move(row)))
      fail("DuplicateCraftFacilityBoundary", id);
    NativeCraftPreparedFacilityRead proof(x.read);
    take_boundary_write_outcome(
        proof.validate_prepared_facility_disposition(old.slot_ref, result));
    return {true, result, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}

}
