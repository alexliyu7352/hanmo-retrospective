#include "craft_facility_boundary.hpp"
#include "craft_job.hpp"
#include "craft_control.hpp"
#include "craft_facility_capacity_plan.hpp"
#include "repair_contract.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace boundary_detail {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
// 解包边界读取结果；失败保留原错误并交由本读取单元的捕获边界处理。
template <class T> T take_boundary_read_outcome(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
// 检查边界读取状态；失败沿用本读取单元的原错误传播路径。
void take_boundary_read_outcome(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take_boundary_read_outcome(compare_world_moments(a, b));
}
bool source(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
bool owner(const CraftOccupancyOwnerRef&a,const CraftOccupancyOwnerRef&b){if(a.index()!=b.index())return false;if(const auto*p=std::get_if<CraftOccupancyJobRef>(&a))return p->job_ref==std::get<CraftOccupancyJobRef>(b).job_ref;return std::get<CraftOccupancyRepairRef>(a).repair_ref==std::get<CraftOccupancyRepairRef>(b).repair_ref;}
const CommonStore&common(const CraftFacilityBoundaryReadContext&x){return x.occupancy.craft.materials.common;}
const CraftJobsState&state(const CraftFacilityBoundaryReadContext&x){return x.occupancy.craft.materials.jobs;}
WorldMoment context(const CraftFacilityBoundaryReadContext &x) {
  if (x.occupancy.boundaries != &x || &x.physical.common != &common(x) ||
      &x.physical.state != &x.occupancy.facility_sources ||
      &x.physical.claims != &x.occupancy.claims)
    fail("CraftFacilityBoundaryContextMismatch");
  const auto at =
      take_boundary_read_outcome(x.occupancy.craft.clock.read_current_moment());
  if (compare(x.physical.current, at) || at.half_day_tick < common(x).tick())
    fail("CraftFacilityBoundaryClockMismatch");
  return at;
}
std::size_t header(const CraftFacilityBoundaryReadContext&x,const SourceRef&r){const auto*h=common(x).state().facts.find(r.id);if(!h||h->id!=r.id||h->domain!=r.domain||h->kind!=r.kind)fail("CraftFacilityBoundaryOriginalHeadRequired",r.id);const auto&order=common(x).state().facts.order();const auto i=std::find(order.begin(),order.end(),r.id);if(i==order.end())fail("CraftFacilityBoundaryOriginalOrderRequired",r.id);return static_cast<std::size_t>(i-order.begin());}
const FacilityClaim2&claim(const CraftFacilityBoundaryReadContext&x,const StableId&id){const FacilityClaim2*out=nullptr;for(const auto&h:x.occupancy.claims.slot_histories)for(const auto&r:h.mutations)for(const auto&c:r.admitted_claims)if(c.claim_id==id){if(out||c.slot_ref!=h.slot_ref)fail("CraftBoundaryDuplicateClaim",id);out=&c;}if(!out)fail("CraftBoundaryOriginalClaimRequired",id);return *out;}
const StableId&previous(const CraftFacilityBoundarySource&r){return std::visit([](const auto&d)->const StableId&{using T=std::decay_t<decltype(d)>;if constexpr(std::is_same_v<T,CraftFacilityUnchangedService>)return d.claim_id;else return d.previous_claim_id;},r.disposition);}
const EconomicFacilityCapacitySource&capacity(const CraftFacilityBoundaryReadContext&x,const FacilityClaim2&c){const EconomicFacilityCapacitySource*out=nullptr;for(const auto&s:x.physical.state.capacity_sources)if(s.slot_ref==c.slot_ref&&s.source_revision==c.capacity_revision){if(out)fail("CraftBoundaryDuplicateCapacity",c.claim_id);out=&s;}if(!out)fail("CraftBoundaryCapacityRequired",c.claim_id);return *out;}
CraftFacilityBusinessSourceView
original_owner(const CraftFacilityBoundaryReadContext &x,
               const CraftOccupancyOwnerRef &o) {
  if (const auto *v = std::get_if<CraftOccupancyJobRef>(&o))
    return take_boundary_read_outcome(read_craft_facility_business_source(
        x.occupancy, {craft_job_source(v->job_ref), 1}));
  return take_boundary_read_outcome(read_craft_facility_business_source(
      x.occupancy,
      {repair_job_source(std::get<CraftOccupancyRepairRef>(o).repair_ref), 1}));
}
MomentOwnerSource terms(const CraftFacilityBusinessSourceView&o){return o.craft_job?MomentOwnerSource{o.craft_job->terms.acceptance_ref,1}:o.repair_job->terms.accepted_service_ref;}
void inputs(const std::vector<MomentOwnerSource>&refs,const StableId&id){if(refs.empty())fail("CraftBoundaryActualInputsRequired",id);for(std::size_t i=0;i<refs.size();++i){if(!valid_source_ref(refs[i].source_ref)||(refs[i].source_schema<1||static_cast<std::uint64_t>(refs[i].source_schema)>std::numeric_limits<std::uint32_t>::max()))fail("CraftBoundaryInvalidInput",id);for(std::size_t k=0;k<i;++k)if(source(refs[i],refs[k]))fail("CraftBoundaryDuplicateInput",id);}}
void cause(const CraftFacilityBoundaryReadContext &x,
           const CraftFacilityBoundarySource &r,
           const CraftFacilityBusinessSourceView &o, const FacilityClaim2 &c) {
  if (const auto *u = std::get_if<CraftFacilityClaimUntil>(&r.cause)) {
    const auto &trigger = claim(x, u->claim_id);
    if (trigger.slot_ref != c.slot_ref ||
        u->admission_receipt_ref.slot_ref != c.slot_ref ||
        compare(trigger.until, r.occurred_at))
      fail("CraftBoundaryNotActualUntil", r.id);
    const auto receipt = take_boundary_read_outcome(read_facility_claim_receipt(
        x.occupancy.claims, u->admission_receipt_ref));
    Integer matches = 0;
    for (const auto &v : receipt.admitted_claims)
      if (v.claim_id == trigger.claim_id)
        ++matches;
    if (matches != 1 ||
        !same_source_ref(receipt.source_result_ref,
                         trigger.source_result_ref) ||
        compare(receipt.occurred_at, trigger.from))
      fail("CraftBoundaryUntilAdmissionMismatch", r.id);
    if (compare(receipt.occurred_at, r.occurred_at) >= 0)
      fail("CraftBoundaryUntilOrderMismatch", r.id);
  } else {
    const auto &ref =
        std::get<CraftFacilityCapacityChanged>(r.cause).actual_change;
    if (ref.source_schema < 1 || !valid_source_ref(ref.source_ref) ||
        ref.source_ref.kind == "facility_retention" ||
        ref.source_ref.kind == "economic_facility_capacity" ||
        ref.source_ref.kind == "craft_facility_boundary")
      fail("CraftBoundaryIndependentCapacityCauseRequired", r.id);
    WorldMoment at;
    if (ref.source_ref.domain == c.slot_ref.provider_domain &&
        ref.source_ref.kind == "facility_condition_change" &&
        ref.source_schema == 1) {
      const auto *v = x.physical.conditions.changes.find(ref.source_ref.id);
      const PhysicalServiceFacilityExtension *f = nullptr;
      for (const auto &row : x.physical.state.facilities)
        if (row.facility_id == c.slot_ref.facility_id) {
          if (f)
            fail("CraftBoundaryDuplicateFacility", r.id);
          f = &row;
        }
      if (!v || v->change_id != ref.source_ref.id || !f ||
          v->facility_ref != f->original_asset_ref ||
          v->old_revision >= v->new_revision)
        fail("CraftBoundaryActualConditionChangeRequired", r.id);
      at = v->occurred_at;
    } else {
      const auto value = take_boundary_read_outcome(
          x.physical.applied.read_applied_capacity_change_basis(ref,
                                                                c.slot_ref));
      if (std::holds_alternative<FacilityInitializedCause>(value) ||
          std::holds_alternative<FacilityRetentionChangedCause>(value))
        fail("CraftBoundaryIndependentChangeRequired", r.id);
      at = take_boundary_read_outcome(
          x.physical.applied.read_applied_facility_change(ref, c.slot_ref));
    }
    if (compare(at, r.occurred_at) ||
        header(x, ref.source_ref) >=
            header(x, craft_facility_boundary_source(r.id)))
      fail("CraftBoundaryCapacityCauseOrderMismatch", r.id);
  }
  if (c.slot_ref != o.slot_ref || c.holder != o.holder || c.units != 1 ||
      std::count(o.original_claim_refs->begin(), o.original_claim_refs->end(),
                 c.claim_id) != 1 ||
      compare(c.until, r.occurred_at) < 0)
    fail("CraftBoundaryOriginalScopeMismatch", r.id);
}
std::optional<CraftFacilityOccupancyView>
placement(const CraftFacilityBoundaryReadContext &x,
          const CraftFacilityBusinessSourceView &o,
          const CraftFacilityBoundarySource &r) {
  std::optional<CraftFacilityOccupancyView> out;
  const auto &refs = o.craft_job ? o.craft_job->facility_occupancy_event_refs
                                 : o.repair_job->facility_occupancy_event_refs;
  const auto cut = header(x, craft_facility_boundary_source(r.id));
  std::size_t last = 0;
  for (const auto &id : refs) {
    const auto pos = header(x, craft_facility_occupancy_source(id));
    if (pos >= cut)
      continue;
    auto value = take_boundary_read_outcome(read_craft_facility_occupancy(
        x.occupancy, craft_facility_occupancy_source(id), 1));
    if (value.slot_ref != o.slot_ref || value.holder != o.holder ||
        compare(value.observed_at, r.occurred_at) > 0 || (out && pos <= last))
      fail("CraftBoundaryPlacementHistoryMismatch", id);
    out = std::move(value);
    last = pos;
  }
  return out;
}
bool same_until(const CraftFacilityClaimUntil&a,const CraftFacilityClaimUntil&b){return a.claim_id==b.claim_id&&a.admission_receipt_ref.slot_ref==b.admission_receipt_ref.slot_ref&&a.admission_receipt_ref.claim_set_revision==b.admission_receipt_ref.claim_set_revision;}
SourceRef boundary_input_cut(const CraftFacilityBoundaryReadContext&x,const CraftFacilityBoundarySource&r){
 auto ref=craft_facility_boundary_source(r.id);auto first=header(x,ref);
 auto include=[&](const SourceRef&source){const auto pos=header(x,source);if(pos<first){first=pos;ref=source;}};
 if(const auto*u=std::get_if<CraftFacilityClaimUntil>(&r.cause))for(const auto&row:state(x).facility_boundaries.rows()){
  const auto*v=std::get_if<CraftFacilityClaimUntil>(&row.second.cause);if(!v||!same_until(*u,*v)||compare(row.second.occurred_at,r.occurred_at))continue;
  include(craft_facility_boundary_source(row.first));
  if(const auto*d=std::get_if<CraftFacilitySuspendedWithoutPlacement>(&row.second.disposition))include(d->actual_control_ref.source_ref);
  if(const auto*d=std::get_if<CraftFacilityRetainedPlacement>(&row.second.disposition);d&&d->actual_control_ref)include(d->actual_control_ref->source_ref);
 }
 return ref;
}
void control(const CraftFacilityBoundaryReadContext &x,
             const CraftFacilityBoundarySource &r,
             const CraftFacilityBusinessSourceView &o,
             const MomentOwnerSource &ref) {
  const auto v = take_boundary_read_outcome(
      read_craft_facility_business_source(x.occupancy, ref));
  if (!owner(v.original_owner, o.original_owner) ||
      v.kind != CraftFacilityBusinessKind::paused ||
      compare(v.occurred_at, r.occurred_at) ||
      header(x, ref.source_ref) >=
          header(x, craft_facility_boundary_source(r.id)))
    fail("CraftBoundaryActualSuspensionRequired", r.id);
  const std::optional<CraftFacilityClaimUntil> *until = nullptr;
  const MomentOwnerSource *actual = nullptr;
  if (o.craft_job) {
    const auto *c = state(x).controls.find(ref.source_ref.id);
    if (!c || !c->actual_cause || !c->actor_id.empty())
      fail("CraftBoundaryActualControlCauseRequired", r.id);
    until = &c->facility_until;
    actual = &*c->actual_cause;
  } else {
    const auto *c =
        x.occupancy.repair->items.repair_jobs.controls.find(ref.source_ref.id);
    if (!c)
      fail("RepairBoundaryActualControlRequired", r.id);
    until = &c->facility_until;
    actual = &c->actual_cause;
  }
  if (const auto *u = std::get_if<CraftFacilityClaimUntil>(&r.cause)) {
    if (!*until || !same_until(**until, *u))
      fail("CraftBoundaryUntilControlIdentityMismatch", r.id);
    take_boundary_read_outcome(validate_craft_facility_until_cause(
        x.occupancy, o.slot_ref, *u, *actual, r.occurred_at));
  } else {
    const auto &changed =
        std::get<CraftFacilityCapacityChanged>(r.cause).actual_change;
    if (*until || actual->source_schema != changed.source_schema ||
        !same_source_ref(actual->source_ref, changed.source_ref))
      fail("CraftBoundaryCapacityControlCauseMismatch", r.id);
  }
}

EconomicFacilitySlotDefinition
boundary_definition(const CraftFacilityBoundaryReadContext &x,
                    const CraftFacilityBoundarySource &r,
                    const FacilityClaim2 &old) {
  if (const auto *c = std::get_if<CraftFacilityCapacityChanged>(&r.cause))
    return take_boundary_read_outcome(
        read_economic_facility_definition_after_original_change(
            x.physical, old.slot_ref, c->actual_change));
  auto end = r.occurred_at;
  if (end.half_day_tick == std::numeric_limits<Integer>::max())
    fail("CraftBoundaryCapacityQueryOverflow", r.id);
  ++end.half_day_tick;
  const auto cap = take_boundary_read_outcome(
      read_economic_facility_capacity_before_owner_source(
          x.physical, old.slot_ref, boundary_input_cut(x, r),
          {r.occurred_at, end}));
  return take_boundary_read_outcome(
      x.occupancy.physical.read_facility_slot_definition_at_source(
          cap.original_capacity_source, old.slot_ref));
}
void window(const CraftFacilityBoundaryReadContext&x,const CraftFacilityBoundarySource&r,const FacilityClaim2&old,const MomentSpan&span,bool retained){const auto d=boundary_definition(x,r,old);const auto limit=retained?d.retention_review_halfdays:std::min(d.max_claim_half_days,x.claim_catalog.max_claim_half_days);if(limit<=0||compare(span.from,r.occurred_at)||compare(span.from,span.to)>=0||span.from.half_day_tick>std::numeric_limits<Integer>::max()-limit)fail("CraftBoundaryInvalidWindow",r.id);auto end=span.from;end.half_day_tick+=limit;if(compare(span.to,end)>0)fail("CraftBoundaryWindowExceedsOriginalRule",r.id);}
void disposition(const CraftFacilityBoundaryReadContext&x,const CraftFacilityBoundarySource&r,const CraftFacilityBusinessSourceView&o,const FacilityClaim2&old){const auto occupied=placement(x,o,r);std::visit([&](const auto&d){using T=std::decay_t<decltype(d)>;
 if constexpr(std::is_same_v<T,CraftFacilityUnchangedService>){if(compare(r.occurred_at,old.until)>=0)fail("CraftBoundaryExpiredClaimUnchanged",r.id);}
 else if constexpr(std::is_same_v<T,CraftFacilityReadmittedService>){if(d.new_claim_id.empty()||d.new_claim_id==old.claim_id||std::count(o.original_claim_refs->begin(),o.original_claim_refs->end(),d.new_claim_id)!=1)fail("CraftBoundaryNewClaimReferenceRequired",r.id);window(x,r,old,d.accepted_window,false);StableId last;for(const auto&u:d.selected_physical_units){if(u.facility_id!=old.slot_ref.facility_id||u.unit_key.empty()||(!last.empty()&&u.unit_key<=last))fail("CraftBoundaryPhysicalSelectionMismatch",r.id);last=u.unit_key;}const auto definition=boundary_definition(x,r,old);if(std::holds_alternative<AggregateFacilityLayout>(definition.physical_layout)?!d.selected_physical_units.empty():d.selected_physical_units.size()!=1)fail("CraftBoundaryPhysicalSelectionSizeMismatch",r.id);}
 else if constexpr(std::is_same_v<T,CraftFacilitySuspendedWithoutPlacement>){control(x,r,o,d.actual_control_ref);if(occupied&&occupied->occupied_units)fail("CraftBoundaryUnclearedPlacementCannotRelease",r.id);}
 else if constexpr(std::is_same_v<T,CraftFacilityRetainedPlacement>){if(d.retained_claim_id.empty()||d.retained_claim_id==old.claim_id||std::count(o.original_claim_refs->begin(),o.original_claim_refs->end(),d.retained_claim_id)!=1)fail("CraftBoundaryRetainedClaimReferenceRequired",r.id);window(x,r,old,d.accepted_review_window,true);if(!occupied||occupied->occupied_units!=old.units||!source({occupied->source_ref,occupied->source_schema},d.actual_occupancy_ref))fail("CraftBoundaryActualPlacementRequired",r.id);if(d.actual_control_ref)control(x,r,o,*d.actual_control_ref);}
 else{if(!occupied||occupied->occupied_units||!source({occupied->source_ref,occupied->source_schema},d.actual_clearance_ref)||compare(occupied->observed_at,r.occurred_at)>0)fail("CraftBoundaryActualClearanceRequired",r.id);}
 },r.disposition);
}
}
using namespace boundary_detail;
SourceRef craft_facility_boundary_source(const StableId&id){return {"crafting","craft_facility_boundary",id};}
Status validate_craft_facility_until_cause(
    const CraftFacilityOccupancyContext &x, const FacilitySlotRef &slot,
    const CraftFacilityClaimUntil &u, const MomentOwnerSource &cause,
    const WorldMoment &at) {
  try {
    if (!x.boundaries || &x.boundaries->occupancy != &x)
      fail("CraftUntilSourceContextRequired", u.claim_id,
           ErrorKind::unsupported_capability);
    const auto &b = *x.boundaries;
    const auto now = context(b);
    if (compare(at, now) > 0 || u.admission_receipt_ref.slot_ref != slot)
      fail("CraftUntilSourceMomentMismatch", u.claim_id);
    const auto episode = take_boundary_read_outcome(
        read_facility_claim_episode(x.claims, u.claim_id, now));
    const auto &original = episode.original_claim;
    if (original.slot_ref != slot || compare(original.until, at) ||
        episode.admission_receipt_ref.slot_ref !=
            u.admission_receipt_ref.slot_ref ||
        episode.admission_receipt_ref.claim_set_revision !=
            u.admission_receipt_ref.claim_set_revision)
      fail("CraftUntilOriginalEpisodeMismatch", u.claim_id);
    const auto admitted = take_boundary_read_outcome(
        read_facility_claim_receipt(x.claims, u.admission_receipt_ref));
    if (admitted.source_schema != cause.source_schema ||
        !same_source_ref(admitted.source_result_ref, cause.source_ref) ||
        !same_source_ref(original.source_result_ref, cause.source_ref) ||
        compare(admitted.occurred_at, original.from) ||
        compare(admitted.occurred_at, at) >= 0)
      fail("CraftUntilOriginalAdmissionCauseMismatch", u.claim_id);
    if (episode.close_receipt_ref) {
      const auto closed = take_boundary_read_outcome(
          read_facility_claim_receipt(x.claims, *episode.close_receipt_ref));
      if (compare(closed.occurred_at, at) < 0)
        fail("CraftUntilAlreadyClosedBeforeBoundary", u.claim_id);
    }
    // 该源证明历史准入，不宣称准入头发生在本次 until M。
    if (cause.source_ref.kind == "facility_retention") {
      const auto *t =
          take_boundary_read_outcome(read_facility_retention_transition(
              x.facility_sources, {cause.source_ref, cause.source_schema},
              slot));
      if (compare(t->occurred_at, admitted.occurred_at))
        fail("CraftUntilRetentionAdmissionMismatch", u.claim_id);
    } else {
      const auto source = take_boundary_read_outcome(
          read_craft_facility_business_source(x, cause));
      if (source.slot_ref != slot ||
          compare(source.occurred_at, admitted.occurred_at))
        fail("CraftUntilBusinessAdmissionMismatch", u.claim_id);
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<CraftFacilityBoundaryView>
read_craft_facility_boundary_for_physical_effect(
    const CraftFacilityBoundaryReadContext &x,
    const MomentOwnerSource &source_ref) {
  try {
    const auto at = context(x);
    if (source_ref.source_schema != 1 ||
        !same_source_ref(source_ref.source_ref, craft_facility_boundary_source(
                                                    source_ref.source_ref.id)))
      fail("UnsupportedCraftFacilityBoundarySource", source_ref.source_ref.id,
           ErrorKind::unsupported_capability);
    const auto *r = state(x).facility_boundaries.find(source_ref.source_ref.id);
    if (!r || r->id != source_ref.source_ref.id ||
        compare(r->occurred_at, at) > 0)
      fail("CraftFacilityBoundaryRequired", source_ref.source_ref.id);
    const auto cut = header(x, source_ref.source_ref);
    auto o = original_owner(x, r->job_or_repair_ref);
    if (!source(terms(o), r->original_terms_ref) ||
        header(x, o.actual_source.source_ref) >= cut ||
        compare(o.occurred_at, r->occurred_at) > 0)
      fail("CraftBoundaryOriginalTermsMismatch", r->id);
    const CraftFacilityBoundarySource *previous = nullptr;
    std::size_t last = 0;
    for (const auto &v : state(x).facility_boundaries.rows())
      if (v.first != r->id &&
          owner(v.second.job_or_repair_ref, r->job_or_repair_ref)) {
        if (v.first != v.second.id)
          fail("CraftBoundaryKeyMismatch", v.first);
        const auto p = header(x, craft_facility_boundary_source(v.first));
        if (p < cut && (!previous || p > last)) {
          previous = &v.second;
          last = p;
        }
      }
    if (r->previous_boundary_ref !=
            (previous ? std::optional<StableId>{previous->id} : std::nullopt) ||
        (previous && compare(previous->occurred_at, r->occurred_at) > 0))
      fail("CraftBoundaryPredecessorMismatch", r->id);
    const auto &c = claim(x, boundary_detail::previous(*r));
    cause(x, *r, o, c);
    inputs(r->adopted_input_refs, r->id);
    auto eligible = o.craft_job
                        ? take_boundary_read_outcome(
                              x.occupancy.craft.original
                                  .read_craft_facility_eligibility_at_boundary(
                                      *o.craft_job, *r))
                        : take_boundary_read_outcome(
                              x.occupancy.repair->original
                                  .read_repair_facility_eligibility_at_boundary(
                                      *o.repair_job, *r));
    const auto &used = std::visit(
        [](const auto &v) -> const std::vector<MomentOwnerSource> & {
          return v.actual_inputs;
        },
        eligible);
    inputs(used, r->id);
    for (const auto &ref : used)
      if (std::none_of(r->adopted_input_refs.begin(),
                       r->adopted_input_refs.end(),
                       [&](const auto &v) { return source(v, ref); }))
        fail("CraftBoundaryInputCoverageMismatch", r->id);
    if (const auto *b = std::get_if<CraftFacilityActuallyBlocked>(&eligible)) {
      if (std::none_of(used.begin(), used.end(), [&](const auto &v) {
            return source(v, b->actual_blocking_source);
          }))
        fail("CraftBoundaryActualBlockingSourceRequired", r->id);
      if (std::holds_alternative<CraftFacilityUnchangedService>(
              r->disposition) ||
          std::holds_alternative<CraftFacilityReadmittedService>(
              r->disposition))
        fail("CraftBoundaryBlockedNormalService", r->id);
    }
    if (const auto *d =
            std::get_if<CraftFacilityReadmittedService>(&r->disposition)) {
      const auto *ok = std::get_if<CraftFacilityEligible>(&eligible);
      if (!ok || !ok->accepted_service_window ||
          compare(ok->accepted_service_window->from, d->accepted_window.from) ||
          compare(ok->accepted_service_window->to, d->accepted_window.to))
        fail("CraftBoundaryOriginalServiceWindowMismatch", r->id);
    }
    disposition(x, *r, o, c);
    return {true, {r, std::move(o), &c, std::move(eligible)}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}

namespace boundary_detail {
bool concerns(const CraftFacilityBoundarySource&r,const StableId&id) {
 if(previous(r)==id)return true;
 if(const auto*d=std::get_if<CraftFacilityReadmittedService>(&r.disposition))return d->new_claim_id==id;
 if(const auto*d=std::get_if<CraftFacilityRetainedPlacement>(&r.disposition))return d->retained_claim_id==id;
 return false;
}
// 原合并来源关系只读真实源字段；收据与经济读叶共用正文，不造临时 claim 收据。
bool root_source_matches(const CraftFacilityBoundaryReadContext &x,
                         const CraftFacilityBoundarySource &r,
                         const SourceRef &source_ref, Integer source_schema,
                         const WorldMoment &occurred_at,
                         const FacilitySlotRef &slot) {
  if (source_schema != 1 || compare(occurred_at, r.occurred_at))
    return false;
  const auto actual = craft_facility_boundary_source(r.id);
  if (same_source_ref(source_ref, actual))
    return true;
  FacilitySourceRef cause{source_ref, source_schema};
  if (source_ref.kind == "facility_retention") {
    const auto *t = take_boundary_read_outcome(
        read_facility_retention_transition(x.physical.state, cause, slot));
    if (compare(t->occurred_at, occurred_at))
      fail("CraftBoundaryRetentionMomentMismatch", r.id);
    cause = {t->cause_ref, t->cause_schema};
  }
  if (const auto *c = std::get_if<CraftFacilityCapacityChanged>(&r.cause))
    return c->actual_change.source_schema == cause.source_schema &&
           same_source_ref(c->actual_change.source_ref, cause.source_ref);
  if (cause.source_schema != 1 || cause.source_ref.domain != "crafting" ||
      cause.source_ref.kind != "craft_facility_boundary")
    return false;
  const auto *root = state(x).facility_boundaries.find(cause.source_ref.id);
  const auto *a = std::get_if<CraftFacilityClaimUntil>(&r.cause);
  const auto *b =
      root ? std::get_if<CraftFacilityClaimUntil>(&root->cause) : nullptr;
  return a && b && a->claim_id == b->claim_id &&
         a->admission_receipt_ref.slot_ref ==
             b->admission_receipt_ref.slot_ref &&
         a->admission_receipt_ref.claim_set_revision ==
             b->admission_receipt_ref.claim_set_revision &&
         !compare(root->occurred_at, r.occurred_at);
}
// 保留原收据路径及错误行为，只将同一原字段关系交给唯一正文。
bool root_matches(const CraftFacilityBoundaryReadContext &x,
                  const CraftFacilityBoundarySource &r,
                  const FacilityClaimMutationReceipt &m,
                  const FacilitySlotRef &slot) {
  return root_source_matches(x, r, m.source_result_ref, m.source_schema,
                             m.occurred_at, slot);
}
struct RetainedPredecessor {
  const FacilityRetentionTransition *transition = nullptr;
  StableId origin;
};
RetainedPredecessor retained_before(const CraftFacilityBoundaryReadContext &x,
                                    const FacilityClaim2 &old) {
  if (old.source_result_ref.kind != "facility_retention")
    return {};
  const auto *t = take_boundary_read_outcome(read_facility_retention_transition(
      x.physical.state, {old.source_result_ref, 1}, old.slot_ref));
  RetainedPredecessor out;
  Integer matches = 0;
  for (const auto &entry : t->entries)
    std::visit(
        [&](const auto &e) {
          using T = std::decay_t<decltype(e)>;
          if constexpr (std::is_same_v<T, FacilityRetentionBegin> ||
                        std::is_same_v<T, FacilityRetentionContinue>)
            if (e.retained_claim_id == old.claim_id) {
              ++matches;
              out = {t, e.origin_claim_id};
            }
          if constexpr (std::is_same_v<T, FacilityRetentionRestoredService>)
            if (e.normal_claim_id == old.claim_id)
              ++matches;
        },
        entry);
  if (matches != 1)
    fail("CraftBoundaryOriginalRetentionIdentityMismatch", old.claim_id);
  return out;
}
FacilityClaimReceiptRef admission(const CraftFacilityBoundaryReadContext&x,const FacilityClaim2&old) {
 std::optional<FacilityClaimReceiptRef>out;
 for(const auto&h:x.occupancy.claims.slot_histories)for(const auto&r:h.mutations)for(const auto&c:r.admitted_claims)if(c.claim_id==old.claim_id){if(out||h.slot_ref!=old.slot_ref)fail("CraftBoundaryDuplicateAdmission",old.claim_id);out=FacilityClaimReceiptRef{h.slot_ref,r.claim_set_revision};}
 if(!out)fail("CraftBoundaryAdmissionRequired",old.claim_id);return *out;
}
std::optional<FacilityRetentionEntry>
retention_entry(const CraftFacilityBoundaryReadContext &x,
                const CraftFacilityBoundaryView &v) {
  const auto &r = *v.original;
  const auto &old = *v.previous_claim;
  const auto prior = retained_before(x, old);
  if (const auto *d =
          std::get_if<CraftFacilityRetainedPlacement>(&r.disposition)) {
    if (prior.transition)
      return FacilityRetentionContinue{
          {old.slot_ref.provider_domain, "facility_retention",
           prior.transition->transition_id},
          prior.origin,
          old.claim_id,
          d->retained_claim_id,
          d->actual_occupancy_ref.source_ref,
          static_cast<std::uint32_t>(d->actual_occupancy_ref.source_schema),
          {},
          d->accepted_review_window};
    return FacilityRetentionBegin{
        old.claim_id,
        admission(x, old),
        old.claim_id,
        d->retained_claim_id,
        d->actual_occupancy_ref.source_ref,
        static_cast<std::uint32_t>(d->actual_occupancy_ref.source_schema),
        d->accepted_review_window};
  }
  if (const auto *d =
          std::get_if<CraftFacilityClearedPlacement>(&r.disposition)) {
    if (!prior.transition)
      return {};
    return FacilityRetentionClear{
        {old.slot_ref.provider_domain, "facility_retention",
         prior.transition->transition_id},
        prior.origin,
        old.claim_id,
        d->actual_clearance_ref.source_ref,
        static_cast<std::uint32_t>(d->actual_clearance_ref.source_schema)};
  }
  if (const auto *d =
          std::get_if<CraftFacilityReadmittedService>(&r.disposition);
      d && prior.transition) {
    std::optional<MomentOwnerSource> resume;
    for (const auto &input : r.adopted_input_refs)
      if (input.source_ref.domain == "crafting" &&
          (input.source_ref.kind == "craft_control" ||
           input.source_ref.kind == "repair_control")) {
        const auto actual = take_boundary_read_outcome(
            read_craft_facility_business_source(x.occupancy, input));
        if (actual.kind != CraftFacilityBusinessKind::resumed)
          continue;
        if (resume || !owner(actual.original_owner, r.job_or_repair_ref) ||
            compare(actual.occurred_at, r.occurred_at) ||
            header(x, input.source_ref) >=
                header(x, craft_facility_boundary_source(r.id)))
          fail("CraftBoundaryActualResumeMismatch", r.id);
        resume = input;
      }
    const auto occupied = placement(x, v.owner, r);
    if (!resume || !occupied || occupied->occupied_units != old.units)
      fail("CraftBoundaryActualResumeRequired", r.id);
    return FacilityRetentionRestoredService{
        {old.slot_ref.provider_domain, "facility_retention",
         prior.transition->transition_id},
        prior.origin,
        old.claim_id,
        d->new_claim_id,
        resume->source_ref,
        static_cast<std::uint32_t>(resume->source_schema),
        occupied->source_ref,
        static_cast<std::uint32_t>(occupied->source_schema)};
  }
  if (prior.transition &&
      !std::holds_alternative<CraftFacilityUnchangedService>(r.disposition))
    fail("CraftBoundaryRetentionDispositionRequired", r.id);
  return {};
}
// 比较同型物理单元的原始安装来源；任一标识或版本不符即不相等。
bool same_units(const std::vector<FacilityPhysicalUnitRef> &a,
                const std::vector<FacilityPhysicalUnitRef> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t n = 0; n < a.size(); ++n) {
    if (a[n].facility_id != b[n].facility_id ||
        a[n].unit_key != b[n].unit_key ||
        a[n].installation_source_ref.index() !=
            b[n].installation_source_ref.index())
      return false;
    if (const auto *i = std::get_if<InstalledPhysicalUnitSource>(
            &a[n].installation_source_ref)) {
      const auto &j =
          std::get<InstalledPhysicalUnitSource>(b[n].installation_source_ref);
      if (i->source_schema != j.source_schema ||
          !same_source_ref(i->source_ref, j.source_ref))
        return false;
    } else {
      const auto &activated =
          std::get<ActivatedPhysicalUnitSource>(a[n].installation_source_ref)
              .original_facility_activation_receipt_ref;
      const auto &j =
          std::get<ActivatedPhysicalUnitSource>(b[n].installation_source_ref)
              .original_facility_activation_receipt_ref;
      if (activated.receipt_id != j.receipt_id ||
          activated.source_schema != j.source_schema)
        return false;
    }
  }
  return true;
}
}
// 经济层只读 S9 实际已发布的原处置集合。完整 typed 源先核验，
// 同一次到期的多 holder 仍按原 cause 关系和 Common 实际顺序列出。
// 这不是整个 slot 的跨专业覆盖结论，也不生成或验证完整 S0 claim 历史。
Outcome<std::vector<FacilitySourceRef>>
read_craft_facility_original_dispositions(
    const CraftFacilityBoundaryReadContext &x, const FacilitySourceRef &root,
    const FacilitySlotRef &slot) {
  try {
    const auto original = take_boundary_read_outcome(
        read_craft_facility_boundary_for_physical_effect(
            x, {root.source_ref, root.source_schema}));
    if (original.owner.slot_ref != slot)
      fail("CraftBoundaryDispositionSlotMismatch", root.source_ref.id);
    std::vector<FacilitySourceRef> result;
    for (const auto &row : state(x).facility_boundaries.rows()) {
      if (!root_source_matches(x, row.second, root.source_ref,
                               root.source_schema,
                               original.original->occurred_at, slot))
        continue;
      const auto source = craft_facility_boundary_source(row.first);
      const auto actual = take_boundary_read_outcome(
          read_craft_facility_boundary_for_physical_effect(x, {source, 1}));
      if (actual.owner.slot_ref != slot || row.first != actual.original->id)
        fail("CraftBoundaryDispositionSlotMismatch", row.first);
      result.push_back({source, 1});
    }
    if (std::count_if(result.begin(), result.end(), [&](const auto &source) {
          return source.source_schema == root.source_schema &&
                 same_source_ref(source.source_ref, root.source_ref);
        }) != 1)
      fail("CraftBoundaryDispositionOriginalCoverageMismatch",
           root.source_ref.id);
    std::sort(result.begin(), result.end(), [&](const auto &a, const auto &b) {
      return header(x, a.source_ref) < header(x, b.source_ref);
    });
    return {true, std::move(result), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::vector<FacilityPhysicalUnitRef>>
read_craft_facility_claim_units(const CraftFacilityBoundaryReadContext &x,
                                const FacilityClaim2 &c) {
  try {
    const auto &original = claim(x, c.claim_id);
    if (original.slot_ref != c.slot_ref ||
        original.capacity_revision != c.capacity_revision ||
        original.holder != c.holder)
      fail("CraftPhysicalClaimIdentityMismatch", c.claim_id);
    const auto &cap = capacity(x, original);
    const auto d = take_boundary_read_outcome(
        x.occupancy.physical.read_facility_slot_definition_at_source(
            {{c.slot_ref.provider_domain, "economic_facility_capacity",
              cap.source_id},
             cap.source_schema},
            c.slot_ref));
    if (std::holds_alternative<AggregateFacilityLayout>(d.physical_layout)) {
      for (const auto &b : x.physical.state.physical_bindings)
        if (b.claim_id == c.claim_id)
          fail("AggregateCraftClaimHasPhysicalBinding", c.claim_id);
      return {true, {}, {}};
    }
    auto proof = take_boundary_read_outcome(
        x.occupancy.physical.read_physical_binding_proof(c.claim_id));
    if (proof.binding.slot_ref != c.slot_ref ||
        proof.binding.physical_unit_refs.size() !=
            static_cast<std::size_t>(c.units))
      fail("CraftPhysicalClaimBindingMismatch", c.claim_id);
    return {true, std::move(proof.binding.physical_unit_refs), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::optional<CraftFacilityBoundaryView>>
read_craft_facility_boundary_for_claim(
    const CraftFacilityBoundaryReadContext &x, const FacilityClaim2 &c,
    const FacilityClaimMutationReceipt &m) {
  try {
    context(x);
    std::optional<CraftFacilityBoundaryView> out;
    for (const auto &row : state(x).facility_boundaries.rows())
      if (concerns(row.second, c.claim_id) &&
          root_matches(x, row.second, m, c.slot_ref)) {
        if (out)
          fail("AmbiguousCraftBoundaryClaimResult", c.claim_id);
        out = take_boundary_read_outcome(
            read_craft_facility_boundary_for_physical_effect(
                x, {craft_facility_boundary_source(row.first), 1}));
      }
    if (m.source_result_ref.domain == "crafting" &&
        m.source_result_ref.kind == "craft_facility_boundary" && !out)
      fail("CraftBoundaryClaimResultMissing", c.claim_id);
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_craft_boundary_claim_admission(const CraftFacilityBoundaryReadContext&x,const CraftFacilityBoundaryView&v,const FacilityClaim2&c,const FacilityClaimMutationReceipt&m) {try {
 const auto&r=*v.original;const auto&old=*v.previous_claim;const MomentSpan*window=nullptr;const StableId*id=nullptr;
 if(const auto*d=std::get_if<CraftFacilityReadmittedService>(&r.disposition)){window=&d->accepted_window;id=&d->new_claim_id;}
 if(const auto*d=std::get_if<CraftFacilityRetainedPlacement>(&r.disposition)){window=&d->accepted_review_window;id=&d->retained_claim_id;}
 if(!id||c.claim_id!=*id||c.slot_ref!=old.slot_ref||c.holder!=old.holder||c.units!=old.units||compare(c.from,window->from)||compare(c.until,window->to)||!same_source_ref(c.source_result_ref,m.source_result_ref)||!root_matches(x,r,m,c.slot_ref))fail("CraftBoundaryAdmissionMismatch",c.claim_id);
 // The sole physical/S0 owner validates capacity_revision and usage rights.
 // A prospective owner result precedes that immutable capacity row.
 if(std::holds_alternative<CraftFacilityRetainedPlacement>(r.disposition)&&!same_source_ref(c.usage_right_ref,old.usage_right_ref))fail("CraftBoundaryRetainedRightMismatch",c.claim_id);
 return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_craft_boundary_claim_close(const CraftFacilityBoundaryReadContext&x,const CraftFacilityBoundaryView&v,const FacilityClaim2&c,FacilityClaimCloseReason why,const FacilityClaimMutationReceipt&m) {try {
 const auto&r=*v.original;if(c.claim_id!=v.previous_claim->claim_id||!root_matches(x,r,m,c.slot_ref)||std::holds_alternative<CraftFacilityUnchangedService>(r.disposition))fail("CraftBoundaryCloseMismatch",c.claim_id);
 const auto expected=std::holds_alternative<CraftFacilitySuspendedWithoutPlacement>(r.disposition)?FacilityClaimCloseReason::suspended:FacilityClaimCloseReason::replaced;
 if(why!=expected)fail("CraftBoundaryCloseReasonMismatch",c.claim_id);
 return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<FacilityRetentionEntry>>
read_craft_boundary_retention_disposition(
    const CraftFacilityBoundaryReadContext &x, const FacilityClaim2 &old,
    const FacilityClaimMutationReceipt &m) {
  try {
    const auto v = take_boundary_read_outcome(
        read_craft_facility_boundary_for_claim(x, old, m));
    if (!v)
      fail("CraftBoundaryOriginalDispositionRequired", old.claim_id,
           ErrorKind::unsupported_capability);
    if (v->previous_claim->claim_id != old.claim_id)
      fail("CraftBoundaryPreviousClaimMismatch", old.claim_id);
    return {true, retention_entry(x, *v), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::vector<FacilityPhysicalUnitRef>>
read_craft_boundary_admission_units(const CraftFacilityBoundaryReadContext &x,
                                    const FacilityClaim2 &c,
                                    const FacilityClaimMutationReceipt &m) {
  try {
    const auto v = take_boundary_read_outcome(
        read_craft_facility_boundary_for_claim(x, c, m));
    if (!v)
      fail("CraftBoundaryOriginalDispositionRequired", c.claim_id,
           ErrorKind::unsupported_capability);
    take_boundary_read_outcome(
        validate_craft_boundary_claim_admission(x, *v, c, m));
    const auto &r = *v->original;
    if (const auto *d =
            std::get_if<CraftFacilityReadmittedService>(&r.disposition)) {
      const auto occupied = placement(x, v->owner, r);
      if (occupied && occupied->occupied_units) {
        const auto old = take_boundary_read_outcome(
            read_craft_facility_claim_units(x, *v->previous_claim));
        if (!same_units(d->selected_physical_units, old))
          fail("CraftBoundaryOccupiedUnitMoved", r.id);
      }
      return {true, d->selected_physical_units, {}};
    }
    if (!std::holds_alternative<CraftFacilityRetainedPlacement>(r.disposition))
      fail("CraftBoundaryAdmissionRequired", c.claim_id);
    return read_craft_facility_claim_units(x, *v->previous_claim);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
namespace boundary_detail {
bool same_retention_entry(const FacilityRetentionEntry&a,const FacilityRetentionEntry&b) {
 if(a.index()!=b.index())return false;
 return std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;const auto&w=std::get<T>(b);
  if(v.origin_claim_id!=w.origin_claim_id||v.previous_claim_id!=w.previous_claim_id||v.holder_source_schema!=w.holder_source_schema)return false;
  if constexpr(std::is_same_v<T,FacilityRetentionBegin>)return v.retained_claim_id==w.retained_claim_id&&v.origin_admission_receipt_ref.slot_ref==w.origin_admission_receipt_ref.slot_ref&&v.origin_admission_receipt_ref.claim_set_revision==w.origin_admission_receipt_ref.claim_set_revision&&same_source_ref(v.holder_occupancy_source_ref,w.holder_occupancy_source_ref)&&!compare(v.retention_window.from,w.retention_window.from)&&!compare(v.retention_window.to,w.retention_window.to);
  else if constexpr(std::is_same_v<T,FacilityRetentionContinue>)return same_source_ref(v.previous_retention_source_ref,w.previous_retention_source_ref)&&v.retained_claim_id==w.retained_claim_id&&same_source_ref(v.holder_occupancy_source_ref,w.holder_occupancy_source_ref)&&!v.clearance_source_ref&&!w.clearance_source_ref&&!compare(v.retention_window.from,w.retention_window.from)&&!compare(v.retention_window.to,w.retention_window.to);
  else if constexpr(std::is_same_v<T,FacilityRetentionClear>)return same_source_ref(v.previous_retention_source_ref,w.previous_retention_source_ref)&&same_source_ref(v.holder_clearance_source_ref,w.holder_clearance_source_ref);
  else return same_source_ref(v.previous_retention_source_ref,w.previous_retention_source_ref)&&v.normal_claim_id==w.normal_claim_id&&same_source_ref(v.actual_resume_result_ref,w.actual_resume_result_ref)&&v.actual_resume_source_schema==w.actual_resume_source_schema&&same_source_ref(v.holder_occupancy_source_ref,w.holder_occupancy_source_ref);
 },a);
}
}
Status validate_craft_boundary_retention_entry(
    const CraftFacilityBoundaryReadContext &x,
    const FacilityRetentionTransition &t, const FacilityRetentionEntry &e) {
  try {
    const auto old_id =
        std::visit([](const auto &v) { return v.previous_claim_id; }, e);
    const auto &old = claim(x, old_id);
    if (t.slot_ref != old.slot_ref || t.source_schema != 1)
      fail("CraftBoundaryRetentionSlotMismatch", old_id);
    Integer count = 0;
    for (const auto &row : state(x).facility_boundaries.rows())
      if (previous(row.second) == old_id &&
          !compare(row.second.occurred_at, t.occurred_at)) {
        const auto &record = row.second;
        bool matches = false;
        if (const auto *c =
                std::get_if<CraftFacilityCapacityChanged>(&record.cause))
          matches = c->actual_change.source_schema == t.cause_schema &&
                    same_source_ref(c->actual_change.source_ref, t.cause_ref);
        else {
          FacilityClaimMutationReceipt root;
          root.source_result_ref = t.cause_ref;
          root.source_schema = t.cause_schema;
          root.occurred_at = t.occurred_at;
          matches = root_matches(x, record, root, t.slot_ref);
        }
        if (!matches)
          continue;
        const auto v = take_boundary_read_outcome(
            read_craft_facility_boundary_for_physical_effect(
                x, {craft_facility_boundary_source(record.id), 1}));
        const auto expected = retention_entry(x, v);
        if (!expected || !same_retention_entry(*expected, e))
          fail("CraftBoundaryRetentionValueMismatch", record.id);
        ++count;
      }
    if (count != 1)
      fail("CraftBoundaryRetentionOriginalCoverageMismatch", old_id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
visit_craft_facility_boundary_pins(const CraftFacilityBoundaryReadContext &x,
                                   const MomentOwnerSource &ref,
                                   CraftFacilityPinSink &sink) {
  try {
    const auto v = take_boundary_read_outcome(
        read_craft_facility_boundary_for_physical_effect(x, ref));
    const auto &r = *v.original;
    take_boundary_read_outcome(sink.source(v.owner.actual_source));
    take_boundary_read_outcome(sink.source(r.original_terms_ref));
    if (r.previous_boundary_ref)
      take_boundary_read_outcome(sink.source(
          {craft_facility_boundary_source(*r.previous_boundary_ref), 1}));
    if (const auto *u = std::get_if<CraftFacilityClaimUntil>(&r.cause))
      take_boundary_read_outcome(
          sink.claim(u->claim_id, u->admission_receipt_ref));
    else
      take_boundary_read_outcome(
          sink.source({std::get<CraftFacilityCapacityChanged>(r.cause)
                           .actual_change.source_ref,
                       std::get<CraftFacilityCapacityChanged>(r.cause)
                           .actual_change.source_schema}));
    take_boundary_read_outcome(sink.claim(v.previous_claim->claim_id,
                                          admission(x, *v.previous_claim)));
    for (const auto &input : r.adopted_input_refs)
      take_boundary_read_outcome(sink.source(input));
    std::visit(
        [&](const auto &d) {
          using T = std::decay_t<decltype(d)>;
          if constexpr (std::is_same_v<T, CraftFacilityReadmittedService>) {
            take_boundary_read_outcome(sink.claim(d.new_claim_id, {}));
            for (const auto &u : d.selected_physical_units)
              std::visit(
                  [&](const auto &i) {
                    using I = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<I,
                                                 InstalledPhysicalUnitSource>)
                      take_boundary_read_outcome(
                          sink.source({i.source_ref, i.source_schema}));
                    else
                      take_boundary_read_outcome(sink.activation(
                          i.original_facility_activation_receipt_ref));
                  },
                  u.installation_source_ref);
          } else if constexpr (std::is_same_v<T,
                                              CraftFacilityRetainedPlacement>) {
            take_boundary_read_outcome(sink.claim(d.retained_claim_id, {}));
            take_boundary_read_outcome(sink.source(d.actual_occupancy_ref));
            if (d.actual_control_ref)
              take_boundary_read_outcome(sink.source(*d.actual_control_ref));
          } else if constexpr (std::is_same_v<
                                   T, CraftFacilitySuspendedWithoutPlacement>)
            take_boundary_read_outcome(sink.source(d.actual_control_ref));
          else if constexpr (std::is_same_v<T, CraftFacilityClearedPlacement>)
            take_boundary_read_outcome(sink.source(d.actual_clearance_ref));
        },
        r.disposition);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
namespace boundary_detail {
CraftFacilityBoundaryView
prepared_source(const CraftFacilityBoundaryReadContext &x,
                const FacilitySlotRef &slot,
                const PreparedFacilityClaimMutation &m) {
  const auto v = take_boundary_read_outcome(
      read_craft_facility_boundary_for_physical_effect(
          x, {m.source_result_ref, m.source_schema}));
  const auto &r = *v.original;
  const auto &old = *v.previous_claim;
  if (old.slot_ref != slot || compare(m.occurred_at, r.occurred_at) ||
      m.claim_set_revision < 1)
    fail("CraftPreparedDispositionIdentityMismatch", r.id);
  const bool unchanged =
      std::holds_alternative<CraftFacilityUnchangedService>(r.disposition);
  const auto *normal =
      std::get_if<CraftFacilityReadmittedService>(&r.disposition);
  const auto *retained =
      std::get_if<CraftFacilityRetainedPlacement>(&r.disposition);
  if (m.closed_claims.size() != (unchanged ? 0u : 1u) ||
      m.admitted_claims.size() != ((normal || retained) ? 1u : 0u))
    fail("CraftPreparedDispositionCoverageMismatch", r.id);
  if (!unchanged) {
    const auto expected =
        std::holds_alternative<CraftFacilitySuspendedWithoutPlacement>(
            r.disposition)
            ? FacilityClaimCloseReason::suspended
            : FacilityClaimCloseReason::replaced;
    if (m.closed_claims[0].claim_id != old.claim_id ||
        m.closed_claims[0].reason != expected)
      fail("CraftPreparedCloseMismatch", r.id);
  }
  if (normal || retained) {
    const auto &c = m.admitted_claims[0];
    const auto &id =
        normal ? normal->new_claim_id : retained->retained_claim_id;
    const auto &w =
        normal ? normal->accepted_window : retained->accepted_review_window;
    if (c.claim_id != id || c.slot_ref != slot || c.holder != old.holder ||
        c.units != old.units || compare(c.from, w.from) ||
        compare(c.until, w.to) ||
        !same_source_ref(c.source_result_ref, m.source_result_ref))
      fail("CraftPreparedAdmissionMismatch", r.id);
    if (retained) {
      if (!same_source_ref(c.usage_right_ref, old.usage_right_ref))
        fail("CraftPreparedRetentionRightMismatch", r.id);
    } else {
      const auto cap = take_boundary_read_outcome(
          read_economic_facility_capacity_before_owner_source(
              x.physical, slot, boundary_input_cut(x, r), w));
      if (!same_source_ref(c.usage_right_ref, cap.usage_right_ref))
        fail("CraftPreparedAdmissionRightMismatch", r.id);
    }
  }
  return v;
}
}
bool NativeCraftPreparedFacilityRead::bound_to(const CommonStore&c,const FacilityClaimsState&s)const noexcept{return &c==&common(x_)&&&s==&x_.occupancy.claims;}
Status NativeCraftPreparedFacilityRead::validate_prepared_facility_disposition(const FacilitySlotRef&slot,const PreparedFacilityClaimMutation&m)const{try{prepared_source(x_,slot,m);return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<FacilityRetentionEntry>> NativeCraftPreparedFacilityRead::read_prepared_facility_retention(const FacilityClaim2&old,const PreparedFacilityClaimMutation&m)const{try{const auto v=prepared_source(x_,old.slot_ref,m);if(v.previous_claim->claim_id!=old.claim_id)fail("CraftPreparedRetentionPreviousMismatch",old.claim_id);return {true,retention_entry(x_,v),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::vector<FacilityPhysicalUnitRef>>
NativeCraftPreparedFacilityRead::read_prepared_facility_admission_units(
    const PreparedFacilityClaimAdmission &c,
    const PreparedFacilityClaimMutation &m) const {
  try {
    const auto v = prepared_source(x_, c.slot_ref, m);
    if (m.admitted_claims.size() != 1)
      fail("CraftPreparedAdmissionRequired", c.claim_id);
    const auto &a = m.admitted_claims[0];
    if (a.claim_id != c.claim_id || a.slot_ref != c.slot_ref ||
        a.holder != c.holder || compare(a.from, c.from) ||
        compare(a.until, c.until) || a.units != c.units ||
        !same_source_ref(a.usage_right_ref, c.usage_right_ref) ||
        !same_source_ref(a.source_result_ref, c.source_result_ref))
      fail("CraftPreparedAdmissionValueMismatch", c.claim_id);
    if (const auto *d = std::get_if<CraftFacilityReadmittedService>(
            &v.original->disposition)) {
      const auto occupied = placement(x_, v.owner, *v.original);
      if (occupied && occupied->occupied_units &&
          !same_units(
              d->selected_physical_units,
              take_boundary_read_outcome(
                  read_craft_facility_claim_units(x_, *v.previous_claim))))
        fail("CraftPreparedOccupiedUnitMoved", c.claim_id);
      return {true, d->selected_physical_units, {}};
    }
    if (!std::holds_alternative<CraftFacilityRetainedPlacement>(
            v.original->disposition))
      fail("CraftPreparedRetentionRequired", c.claim_id);
    return read_craft_facility_claim_units(x_, *v.previous_claim);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status visit_craft_facility_until_control_pins(
    const CraftFacilityBoundaryReadContext &x, const MomentOwnerSource &ref,
    CraftFacilityPinSink &sink) {
  try {
    const auto v = take_boundary_read_outcome(
        read_craft_facility_business_source(x.occupancy, ref));
    const CraftFacilityClaimUntil *u = nullptr;
    const MomentOwnerSource *actual = nullptr;
    if (ref.source_ref.kind == "craft_control") {
      const auto *c = state(x).controls.find(ref.source_ref.id);
      if (c && c->facility_until && c->actual_cause) {
        u = &*c->facility_until;
        actual = &*c->actual_cause;
      }
    } else if (ref.source_ref.kind == "repair_control" && x.occupancy.repair) {
      const auto *c = x.occupancy.repair->items.repair_jobs.controls.find(
          ref.source_ref.id);
      if (c && c->facility_until) {
        u = &*c->facility_until;
        actual = &c->actual_cause;
      }
    }
    if (!u || !actual)
      fail("CraftActualUntilControlRequired", ref.source_ref.id);
    take_boundary_read_outcome(validate_craft_facility_until_cause(
        x.occupancy, v.slot_ref, *u, *actual, v.occurred_at));
    take_boundary_read_outcome(sink.source(*actual));
    take_boundary_read_outcome(
        sink.claim(u->claim_id, u->admission_receipt_ref));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
// 完整发布时沿原 Common 前缀重放优先序；不从今天的 Job phase 倒证旧处置。
namespace boundary_detail {
bool working_before_change(const CraftFacilityBoundaryReadContext &x,
                           const CraftFacilityBoundaryView &v,
                           const SourceRef &change) {
  const auto cut = header(x, change);
  bool working = true;
  if (v.owner.craft_job) {
    const auto &job = *v.owner.craft_job;
    const auto *recipe = take_boundary_read_outcome(read_craft_recipe(
        x.occupancy.craft.materials.recipes, job.terms.recipe_ref));
    const auto required =
        take_boundary_read_outcome(craft_required_normal_thirds(*recipe));
    const auto &order = common(x).state().facts.order();
    for (std::size_t index = 0; index < cut; ++index) {
      const auto &id = order[index];
      if (const auto *c = state(x).controls.find(id);
          c && c->job_id == job.id) {
        if (c->id != id || std::count(job.control_refs.begin(),
                                      job.control_refs.end(), id) != 1)
          fail("CraftOriginalControlPrefixMismatch", id);
        working = c->kind == CraftControlKind::resume;
      }
      if (const auto *g = state(x).gates.find(id); g && g->job_id == job.id) {
        take_boundary_read_outcome(
            read_craft_gate_for_original_effect(x.occupancy.craft, id));
        if (g->result.work_units_after == required)
          working = false;
      }
      if ((job.cancellation_ref && id == *job.cancellation_ref) ||
          (job.cancelled_result_ref && id == *job.cancelled_result_ref) ||
          (job.completion_fact_id && id == *job.completion_fact_id))
        working = false;
    }
  } else {
    const auto &job = *v.owner.repair_job;
    const auto &order = common(x).state().facts.order();
    for (std::size_t index = 0; index < cut; ++index) {
      const auto &id = order[index];
      if (const auto *c =
              x.occupancy.repair->items.repair_jobs.controls.find(id);
          c && c->job_id == job.id) {
        if (c->id != id || std::count(job.control_refs.begin(),
                                      job.control_refs.end(), id) != 1)
          fail("RepairOriginalControlPrefixMismatch", id);
        working = c->kind != RepairControlKind::suspended;
      }
      if ((job.completed_ref && id == *job.completed_ref) ||
          (job.interrupted_ref && id == *job.interrupted_ref))
        working = false;
    }
  }
  return working;
}
bool started_before_change(const CraftFacilityBoundaryReadContext &x,
                           const CraftFacilityBoundaryView &v,
                           const SourceRef &change) {
  const auto &old = *v.previous_claim;
  const auto at = v.original->occurred_at;
  const auto cut = header(x, change);
  if (compare(old.from, at) > 0)
    return false;
  const auto &events = v.owner.craft_job
                           ? v.owner.craft_job->facility_occupancy_event_refs
                           : v.owner.repair_job->facility_occupancy_event_refs;
  for (const auto &id : events) {
    if (header(x, craft_facility_occupancy_source(id)) >= cut)
      continue;
    const auto occupied =
        take_boundary_read_outcome(read_craft_facility_occupancy(
            x.occupancy, craft_facility_occupancy_source(id), 1));
    if (compare(occupied.observed_at, at) > 0)
      fail("CraftFuturePlacementBeforeChange", id);
    if (occupied.occupied_units)
      return true;
  }
  if (v.owner.craft_job)
    for (const auto &id : v.owner.craft_job->actual_service_refs) {
      const auto *s = take_boundary_read_outcome(
          read_craft_service_for_original_effect(x.occupancy.craft, id));
      if (header(x, SourceRef{"crafting", "craft_service_span", id}) >= cut)
        continue;
      if (std::find(s->actual.facility_claim_refs.begin(),
                    s->actual.facility_claim_refs.end(),
                    old.claim_id) != s->actual.facility_claim_refs.end() &&
          compare(s->actual.actual_span.from, at) < 0)
        return true;
    }
  return false;
}
std::vector<const FacilityClaim2*> claims_before(const CraftFacilityBoundaryReadContext&x,const FacilitySlotRef&slot,Integer revision){
 std::vector<const FacilityClaim2*>out;const FacilitySlotHistory*history=nullptr;
 for(const auto&h:x.occupancy.claims.slot_histories)if(h.slot_ref==slot){if(history)fail("CraftDuplicateSlotHistory",slot.facility_id);history=&h;}
 if(!history)fail("CraftOriginalSlotHistoryRequired",slot.facility_id);
 bool found=false;for(const auto&m:history->mutations){if(m.claim_set_revision==revision){found=true;break;}if(m.claim_set_revision>revision)fail("CraftOriginalRevisionMissing",slot.facility_id);
  for(const auto&closed:m.closed_claims){auto it=std::find_if(out.begin(),out.end(),[&](const auto*c){return c->claim_id==closed.claim_id;});if(it==out.end())fail("CraftOriginalClosedClaimMissing",closed.claim_id);out.erase(it);}
  for(const auto&c:m.admitted_claims){if(std::any_of(out.begin(),out.end(),[&](const auto*v){return v->claim_id==c.claim_id;}))fail("CraftDuplicateOriginalClaim",c.claim_id);out.push_back(&c);}
 }
 if(!found)fail("CraftActualMutationRevisionRequired",slot.facility_id);return out;
}
void slot_priority(const CraftFacilityBoundaryReadContext &x,
                   const FacilitySlotRef &slot,
                   const CraftFacilityBoundarySource &pass,
                   const FacilityClaimMutationReceipt &mutation) {
  const auto *changed = std::get_if<CraftFacilityCapacityChanged>(&pass.cause);
  const auto *until = std::get_if<CraftFacilityClaimUntil>(&pass.cause);
  const auto cut =
      changed ? changed->actual_change.source_ref : boundary_input_cut(x, pass);
  struct Row {
    CraftFacilityBoundaryView source;
    bool started = false, working = false;
    std::optional<CraftFacilityOccupancyView> occupied;
    std::vector<FacilityPhysicalUnitRef> units;
  };
  std::vector<Row> rows;
  for (const auto *old : claims_before(x, slot, mutation.claim_set_revision)) {
    const CraftFacilityBoundarySource *record = nullptr;
    for (const auto &entry : state(x).facility_boundaries.rows())
      if (previous(entry.second) == old->claim_id &&
          !compare(entry.second.occurred_at, mutation.occurred_at)) {
        bool matches = false;
        if (changed) {
          const auto *c =
              std::get_if<CraftFacilityCapacityChanged>(&entry.second.cause);
          matches = c &&
                    c->actual_change.source_schema ==
                        changed->actual_change.source_schema &&
                    same_source_ref(c->actual_change.source_ref,
                                    changed->actual_change.source_ref);
        } else {
          const auto *u =
              std::get_if<CraftFacilityClaimUntil>(&entry.second.cause);
          matches = u && until && same_until(*u, *until);
        }
        if (!matches)
          continue;
        if (record)
          fail("DuplicateCraftOriginalFacilityDisposition", old->claim_id);
        record = &entry.second;
      }
    if (!record)
      fail("CraftWholeSlotOriginalOwnerRequired", old->claim_id,
           ErrorKind::unsupported_capability);
    Row row;
    row.source = take_boundary_read_outcome(
        read_craft_facility_boundary_for_physical_effect(
            x, {craft_facility_boundary_source(record->id), 1}));
    row.started = started_before_change(x, row.source, cut);
    row.working = working_before_change(x, row.source, cut);
    row.occupied = placement(x, row.source.owner, *record);
    row.units =
        take_boundary_read_outcome(read_craft_facility_claim_units(x, *old));
    rows.push_back(std::move(row));
  }
  if (rows.empty())
    fail("CraftOriginalFacilityPassEmpty", pass.id);
  std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
    if (a.started != b.started)
      return a.started;
    const auto order =
        compare(a.source.previous_claim->from, b.source.previous_claim->from);
    return order ? order < 0
                 : a.source.previous_claim->claim_id <
                       b.source.previous_claim->claim_id;
  });
  const auto d =
      boundary_definition(x, pass, *rows.front().source.previous_claim);
  auto query_end = mutation.occurred_at;
  if (d.retention_review_halfdays < 1 ||
      query_end.half_day_tick >
          std::numeric_limits<Integer>::max() - d.retention_review_halfdays)
    fail("CraftRetentionQueryOverflow", slot.facility_id);
  query_end.half_day_tick += d.retention_review_halfdays;
  for (const auto &row : rows) {
    if (compare(row.source.previous_claim->until, query_end) > 0)
      query_end = row.source.previous_claim->until;
    if (const auto *e =
            std::get_if<CraftFacilityEligible>(&row.source.actual_eligibility);
        e && e->accepted_service_window &&
        compare(e->accepted_service_window->to, query_end) > 0)
      query_end = e->accepted_service_window->to;
  }
  EconomicFacilityServiceCapacityView cap;
  bool whole = false;
  if (changed) {
    cap = take_boundary_read_outcome(
        read_economic_facility_capacity_at_original_change(
            x.physical, slot, changed->actual_change,
            {mutation.occurred_at, query_end}));
    whole = take_boundary_read_outcome(
                x.physical.applied.read_applied_whole_slot_restriction(
                    changed->actual_change, slot))
                .has_value();
  } else {
    const auto original = take_boundary_read_outcome(
        read_economic_facility_capacity_before_owner_source(
            x.physical, slot, cut, {mutation.occurred_at, query_end}));
    cap = original;
    const EconomicFacilityCapacitySource *record = nullptr;
    for (const auto &r : x.physical.state.capacity_sources)
      if (r.source_id == original.original_capacity_source.source_ref.id) {
        if (record)
          fail("CraftUntilDuplicateCapacitySource", r.source_id);
        record = &r;
      }
    if (!record)
      fail("CraftUntilCapacitySourceRequired", pass.id);
    if (const auto *r =
            std::get_if<EntireSlotNoNewAdmission>(&record->admission_mode))
      whole = take_boundary_read_outcome(
                  x.physical.applied.read_applied_whole_slot_restriction(
                      {r->whole_slot_restriction_source_ref,
                       r->whole_slot_restriction_source_schema},
                      slot))
                  .has_value();
  }
  Integer occupied_units = 0;
  if (!std::holds_alternative<IndependentFacilityUnits>(d.physical_layout))
    for (const auto &row : rows)
      if (row.occupied && row.occupied->occupied_units &&
          compare(row.source.previous_claim->from, mutation.occurred_at) <= 0) {
        if (occupied_units > std::numeric_limits<Integer>::max() -
                                 row.source.previous_claim->units)
          fail("CraftFacilityOccupancyOverflow", slot.facility_id);
        occupied_units += row.source.previous_claim->units;
      }
  whole =
      whole ||
      (!std::holds_alternative<IndependentFacilityUnits>(d.physical_layout) &&
       occupied_units > cap.capacity);
  std::vector<craft_capacity_plan::Prior> earlier;
  std::size_t last_order = 0;
  for (std::size_t n = 0; n < rows.size(); ++n) {
    const auto &row = rows[n];
    const auto &r = *row.source.original;
    const auto &old = *row.source.previous_claim;
    bool working = row.working;
    for (std::size_t k = 0; k < n; ++k)
      if (owner(rows[k].source.owner.original_owner,
                row.source.owner.original_owner)) {
        const auto &prior = *rows[k].source.original;
        if (std::holds_alternative<CraftFacilitySuspendedWithoutPlacement>(
                prior.disposition))
          working = false;
        else if (const auto *d = std::get_if<CraftFacilityRetainedPlacement>(
                     &prior.disposition);
                 d && d->actual_control_ref)
          working = false;
      }
    const auto order = header(x, craft_facility_boundary_source(r.id));
    if (n && order <= last_order)
      fail("CraftCapacityOriginalPriorityOrderMismatch", r.id);
    last_order = order;
    std::vector<FacilityPhysicalUnitRef> later;
    for (std::size_t k = n + 1; k < rows.size(); ++k)
      if (rows[k].started)
        later.insert(later.end(), rows[k].units.begin(), rows[k].units.end());
    const auto plan = take_boundary_read_outcome(craft_capacity_plan::choose(
        {old, r.occurred_at, cap, d,
         std::get_if<CraftFacilityEligible>(&row.source.actual_eligibility),
         working, retained_before(x, old).transition != nullptr,
         row.occupied && row.occupied->occupied_units > 0,
         row.occupied && row.occupied->occupied_units == 0, whole, row.units,
         earlier, later, x.claim_catalog.max_claim_half_days}));
    const auto expected = static_cast<std::size_t>(plan.kind);
    if (r.disposition.index() != expected)
      fail("CraftCapacityOriginalDispositionMismatch", r.id);
    if (const auto *a =
            std::get_if<CraftFacilityReadmittedService>(&r.disposition)) {
      if (!plan.window || compare(a->accepted_window.from, plan.window->from) ||
          compare(a->accepted_window.to, plan.window->to) ||
          !same_units(a->selected_physical_units, plan.physical_units))
        fail("CraftCapacityOriginalReadmissionMismatch", r.id);
    }
    if (const auto *a =
            std::get_if<CraftFacilityRetainedPlacement>(&r.disposition)) {
      if (!plan.window ||
          compare(a->accepted_review_window.from, plan.window->from) ||
          compare(a->accepted_review_window.to, plan.window->to) ||
          a->actual_control_ref.has_value() != working)
        fail("CraftCapacityOriginalRetentionMismatch", r.id);
    }
    if (plan.kind == craft_capacity_plan::Disposition::unchanged)
      earlier.push_back({{old.from, old.until}, old.units, false, row.units});
    else if (plan.kind == craft_capacity_plan::Disposition::readmitted ||
             plan.kind == craft_capacity_plan::Disposition::retained) {
      const StableId &id =
          plan.kind == craft_capacity_plan::Disposition::readmitted
              ? std::get<CraftFacilityReadmittedService>(r.disposition)
                    .new_claim_id
              : std::get<CraftFacilityRetainedPlacement>(r.disposition)
                    .retained_claim_id;
      const FacilityClaim2 *adopted = nullptr;
      for (const auto &c : mutation.admitted_claims)
        if (c.claim_id == id) {
          if (adopted)
            fail("CraftDuplicateAdmittedClaim", id);
          adopted = &c;
        }
      if (!adopted)
        fail("CraftActualCapacityAdmissionRequired", id);
      if (changed && adopted->capacity_revision != cap.capacity_revision)
        fail("CraftActualCapacityRevisionMismatch", id);
      const auto &actual_capacity = capacity(x, *adopted);
      if (std::none_of(
              mutation.capacity_source_refs.begin(),
              mutation.capacity_source_refs.end(), [&](const auto &source) {
                return source.source_schema == actual_capacity.source_schema &&
                       same_source_ref(source.source_ref,
                                       SourceRef{slot.provider_domain,
                                                 "economic_facility_capacity",
                                                 actual_capacity.source_id});
              }))
        fail("CraftActualAdmittedCapacitySourceRequired", id);
      earlier.push_back(
          {{adopted->from, adopted->until},
           adopted->units,
           plan.kind == craft_capacity_plan::Disposition::retained,
           plan.physical_units});
    }
  }
}
}
Status
validate_craft_facility_boundaries(const CraftFacilityBoundaryReadContext &x) {
  try {
    context(x);
    for (const auto &head : common(x).state().facts.rows())
      if (head.second.domain == "crafting" &&
          head.second.kind == "craft_facility_boundary" &&
          !state(x).facility_boundaries.find(head.first))
        fail("CraftBoundaryPayloadRequired", head.first);
    for (const auto &row : state(x).facility_boundaries.rows()) {
      const auto v = take_boundary_read_outcome(
          read_craft_facility_boundary_for_physical_effect(
              x, {craft_facility_boundary_source(row.first), 1}));
      const auto &r = *v.original;
      Integer matches = 0;
      for (const auto &h : x.occupancy.claims.slot_histories)
        if (h.slot_ref == v.owner.slot_ref)
          for (const auto &m : h.mutations)
            if (root_matches(x, r, m, h.slot_ref)) {
              Integer closes = 0, admits = 0;
              for (const auto &closed : m.closed_claims)
                if (closed.claim_id == v.previous_claim->claim_id) {
                  ++closes;
                  take_boundary_read_outcome(
                      validate_craft_boundary_claim_close(
                          x, v, *v.previous_claim, closed.reason, m));
                }
              for (const auto &c : m.admitted_claims)
                if (concerns(r, c.claim_id) &&
                    c.claim_id != v.previous_claim->claim_id) {
                  ++admits;
                  take_boundary_read_outcome(
                      validate_craft_boundary_claim_admission(x, v, c, m));
                  const auto selected = take_boundary_read_outcome(
                      read_craft_boundary_admission_units(x, c, m));
                  const auto physical = take_boundary_read_outcome(
                      read_craft_facility_claim_units(x, c));
                  if (!same_units(selected, physical))
                    fail("CraftBoundaryPhysicalAdoptionMismatch", r.id);
                }
              const bool unchanged =
                  std::holds_alternative<CraftFacilityUnchangedService>(
                      r.disposition);
              const bool readmitted =
                  std::holds_alternative<CraftFacilityReadmittedService>(
                      r.disposition) ||
                  std::holds_alternative<CraftFacilityRetainedPlacement>(
                      r.disposition);
              if (closes != (unchanged ? 0 : 1) ||
                  admits != (readmitted ? 1 : 0))
                fail("CraftBoundarySharedAdoptionMismatch", r.id);
              if (m.source_result_ref.kind == "facility_retention") {
                const auto *t = take_boundary_read_outcome(
                    read_facility_retention_transition(
                        x.physical.state,
                        {m.source_result_ref, m.source_schema}, h.slot_ref));
                const auto expected = retention_entry(x, v);
                Integer found = 0;
                for (const auto &e : t->entries)
                  if (std::visit(
                          [&](const auto &entry) {
                            return entry.previous_claim_id ==
                                   v.previous_claim->claim_id;
                          },
                          e)) {
                    if (!expected || !same_retention_entry(*expected, e))
                      fail("CraftBoundaryRetentionValueMismatch", r.id);
                    ++found;
                  }
                if (found != (expected ? 1 : 0))
                  fail("CraftBoundaryRetentionCoverageMismatch", r.id);
              } else if (retention_entry(x, v))
                fail("CraftBoundaryRetentionRootRequired", r.id);
              if (m.capacity_source_refs.empty())
                fail("CraftBoundaryCapacityAdoptionRequired", r.id);
              if (const auto *changed =
                      std::get_if<CraftFacilityCapacityChanged>(&r.cause)) {
                bool first = true;
                for (const auto &other : state(x).facility_boundaries.rows()) {
                  const auto *c = std::get_if<CraftFacilityCapacityChanged>(
                      &other.second.cause);
                  if (c &&
                      claim(x, previous(other.second)).slot_ref == h.slot_ref &&
                      c->actual_change.source_schema ==
                          changed->actual_change.source_schema &&
                      same_source_ref(c->actual_change.source_ref,
                                      changed->actual_change.source_ref) &&
                      !compare(other.second.occurred_at, r.occurred_at) &&
                      header(x, craft_facility_boundary_source(other.first)) <
                          header(x, craft_facility_boundary_source(r.id))) {
                    first = false;
                    break;
                  }
                }
                if (first)
                  slot_priority(x, h.slot_ref, r, m);
              } else if (const auto *until =
                             std::get_if<CraftFacilityClaimUntil>(&r.cause)) {
                bool first = true;
                for (const auto &other : state(x).facility_boundaries.rows()) {
                  const auto *u =
                      std::get_if<CraftFacilityClaimUntil>(&other.second.cause);
                  if (u && same_until(*u, *until) &&
                      !compare(other.second.occurred_at, r.occurred_at) &&
                      header(x, craft_facility_boundary_source(other.first)) <
                          header(x, craft_facility_boundary_source(r.id))) {
                    first = false;
                    break;
                  }
                }
                if (first)
                  slot_priority(x, h.slot_ref, r, m);
              }
              ++matches;
            }
      if (matches != 1)
        fail("CraftBoundaryPublicationCoverageMismatch", r.id);
    }
    auto covered_control = [&](const MomentOwnerSource &ref,
                               const CraftFacilityClaimUntil &until,
                               const WorldMoment &at) {
      Integer count = 0;
      for (const auto &row : state(x).facility_boundaries.rows()) {
        const auto *u = std::get_if<CraftFacilityClaimUntil>(&row.second.cause);
        if (!u || !same_until(*u, until) || compare(row.second.occurred_at, at))
          continue;
        const MomentOwnerSource *control_ref = nullptr;
        if (const auto *d = std::get_if<CraftFacilitySuspendedWithoutPlacement>(
                &row.second.disposition))
          control_ref = &d->actual_control_ref;
        else if (const auto *d = std::get_if<CraftFacilityRetainedPlacement>(
                     &row.second.disposition);
                 d && d->actual_control_ref)
          control_ref = &*d->actual_control_ref;
        if (control_ref && source(*control_ref, ref))
          ++count;
      }
      if (count != 1)
        fail("CraftUntilControlBoundaryCoverageMismatch", ref.source_ref.id);
    };
    for (const auto &row : state(x).controls.rows())
      if (row.second.facility_until)
        covered_control({craft_control_source(row.first), 1},
                        *row.second.facility_until, row.second.occurred_at);
    if (x.occupancy.repair)
      for (const auto &row :
           x.occupancy.repair->items.repair_jobs.controls.rows())
        if (row.second.facility_until)
          covered_control({repair_control_source(row.first), 1},
                          *row.second.facility_until, row.second.occurred_at);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
}
